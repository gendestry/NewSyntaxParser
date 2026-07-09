#pragma once
// =============================================================================
//  All.h — STUDY / REFERENCE FILE.
//
//  The entire Tree/ AST + interpreter condensed into one self-contained header,
//  in dependency order, so the whole pipeline reads top-to-bottom:
//
//    1. builder     : CST (Parsing::Syntax::Node)  ->  Sel:: typed AST
//    2. Context     : interpreter state
//    3. Resolver    : one Selector -> the set of fixture ids it denotes
//    4. Interpreter : walks the program, mutating the Context, applying guards
//
//  This file is NOT part of the build — the real code lives in the individual
//  Builder/Context/Resolver/Interpreter .h/.cpp pairs. Everything here is
//  `inline` so it could compile if included, but nothing includes it.
// =============================================================================

#include "Ast.gen.h"      // Sel:: Selector / Command / Item / AtValue (generated)
#include "Syntax/Node.h"  // Parsing::Syntax::Node (the CST node type)

#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Sel {
using Node = Parsing::Syntax::Node;

// ============================================================================
//  1. CST -> AST builder
//
//  Reminder on how the engine flattens the parse tree:
//    Terminal/Literal -> leaf (carries a token);   RuleRef -> child node;
//    Seq/Star/Optional/( ) -> flatten into the parent;   Choice -> only the
//    winning alternative appears. So each build* reads its node's `kids` by
//    position, following the shape of the matching rule in lang.syn.
// ============================================================================

// A NUM leaf may carry a decimal ('.' [0-9]+)?; parse once, use as int or real.
inline double numOf(const Node &leaf) { return std::stod(leaf.token->value); }
inline long long intOf(const Node &leaf) {
  return static_cast<long long>(numOf(leaf));
}

// grpsel : GROUP NUM              kids = [GROUP, NUM]
inline std::unique_ptr<Group> buildGrpsel(const Node &g) {
  auto n = std::make_unique<Group>();
  n->id = intOf(g.kids[1]);
  return n;
}

// presetsel : PRESET NUM DOT NUM  kids = [PRESET, NUM, DOT, NUM]
inline std::unique_ptr<Preset> buildPresetsel(const Node &p) {
  auto n = std::make_unique<Preset>();
  n->bank = intOf(p.kids[1]);
  n->number = intOf(p.kids[3]);
  return n;
}

// fixsel : NUM (THRU NUM)?        kids = [NUM] | [NUM, THRU, NUM]
inline SelectorPtr buildFixsel(const Node &f) {
  if (f.kids.size() == 1) {
    auto n = std::make_unique<Fixture>();
    n->id = intOf(f.kids[0]);
    return n;
  }
  auto n = std::make_unique<FixtureRange>();
  n->from = intOf(f.kids[0]);
  n->to = intOf(f.kids[2]);
  return n;
}

// sel : fixsel | grpsel           kids = [ chosen ]
inline SelectorPtr buildSel(const Node &sel) {
  const Node &c = sel.kids[0];
  if (c.rule == "fixsel")
    return buildFixsel(c);
  if (c.rule == "grpsel")
    return buildGrpsel(c);
  throw std::runtime_error("buildSel: unexpected '" + c.rule + "'");
}

// modsel : grpsel | presetsel     kids = [ chosen ]
inline SelectorPtr buildModsel(const Node &m) {
  const Node &c = m.kids[0];
  if (c.rule == "grpsel")
    return buildGrpsel(c);
  if (c.rule == "presetsel")
    return buildPresetsel(c);
  throw std::runtime_error("buildModsel: unexpected '" + c.rule + "'");
}

// at : AT (NUM | presetsel)       kids = [AT, NUM | presetsel]
inline AtValue buildAt(const Node &at) {
  AtValue v;
  const Node &arg = at.kids[1];
  if (arg.rule == "presetsel") {
    v.isPreset = true;
    v.preset = buildPresetsel(arg);
  } else { // NUM leaf
    v.isPreset = false;
    v.level = numOf(arg);
  }
  return v;
}

// selection : sel (op sel)*       kids = [sel, op, sel, ...]  (flattened)
inline void buildItems(const Node &selection, std::vector<Item> &out) {
  std::string pendingOp; // empty for the first item (no leading operator)
  for (const Node &k : selection.kids) {
    if (k.rule == "op") {
      pendingOp = k.kids[0].token->value; // PLUS / MINUS leaf
    } else if (k.rule == "sel") {
      Item item;
      item.op = pendingOp;
      item.sel = buildSel(k);
      out.push_back(std::move(item));
      pendingOp.clear();
    }
  }
}

// command : selection at? | store | delete | clear | at
//   kids = [ chosen-rule-node, (at)? ]
inline CommandPtr buildCommand(const Node &command) {
  const Node &first = command.kids[0];

  if (first.rule == "selection") {
    auto c = std::make_unique<SelectCmd>();
    buildItems(first, c->items);
    if (command.kids.size() > 1 && command.kids[1].rule == "at") {
      c->hasAt = true;
      c->at = buildAt(command.kids[1]);
    }
    return c;
  }
  if (first.rule == "store") { // store : STORE modsel  kids=[STORE, modsel]
    auto c = std::make_unique<StoreCmd>();
    c->target = buildModsel(first.kids[1]);
    return c;
  }
  if (first.rule == "delete") { // delete : DELETE modsel
    auto c = std::make_unique<DeleteCmd>();
    c->target = buildModsel(first.kids[1]);
    return c;
  }
  if (first.rule == "clear")
    return std::make_unique<ClearCmd>();
  if (first.rule == "at") { // bare at
    auto c = std::make_unique<AtCmd>();
    c->at = buildAt(first);
    return c;
  }
  throw std::runtime_error("buildCommand: unexpected '" + first.rule + "'");
}

// entry : command+               kids = [command, ...]
inline Program buildProgram(const Node &entry) {
  Program prog;
  for (const Node &cmd : entry.kids)
    if (cmd.rule == "command")
      prog.push_back(buildCommand(cmd));
  return prog;
}

// ============================================================================
//  2. Interpreter state
// ============================================================================
struct Context {
  static constexpr int MAX_FIXTURES = 20;

  std::set<int> selected;                               // active fixture ids
  std::map<int, std::set<int>> groups;                  // group id  -> fixtures
  std::map<std::pair<int, int>, std::set<int>> presets; // (bank,num) -> fixtures
  std::array<float, MAX_FIXTURES + 1> level{};          // fixture 1..20 -> level

  bool hasSelection() const { return !selected.empty(); }
};

// Render a set of ids as {a, b, c}.
inline std::string fmt(const std::set<int> &s) {
  std::string out = "{";
  bool first = true;
  for (int id : s) {
    if (!first)
      out += ", ";
    out += std::to_string(id);
    first = false;
  }
  return out + "}";
}

// ============================================================================
//  3. Selector resolver: ONE selector -> the fixture ids it denotes.
//     (Groups/presets are looked up in the Context.)
// ============================================================================
struct Resolver : SelectorVisitor {
  const Context &ctx;
  std::set<int> ids; // result of the last resolve()

  explicit Resolver(const Context &c) : ctx(c) {}

  std::set<int> resolve(Selector &s) {
    ids.clear();
    s.accept(*this); // dispatches to one of the visit() overloads below
    return ids;
  }

  void visit(Fixture &f) override { ids = {static_cast<int>(f.id)}; }
  void visit(FixtureRange &r) override {
    for (int i = static_cast<int>(r.from); i <= static_cast<int>(r.to); ++i)
      ids.insert(i);
  }
  void visit(Group &g) override {
    if (auto it = ctx.groups.find(static_cast<int>(g.id)); it != ctx.groups.end())
      ids = it->second;
  }
  void visit(Preset &p) override {
    auto key = std::pair{static_cast<int>(p.bank), static_cast<int>(p.number)};
    if (auto it = ctx.presets.find(key); it != ctx.presets.end())
      ids = it->second;
  }
};

// ============================================================================
//  4. Command interpreter.
//     Guards: store/delete/at require a non-empty selection; clear is always
//     allowed. selection itself always runs and (re)sets ctx.selected.
// ============================================================================
struct Interpreter : CommandVisitor {
  Context ctx;

  // Execute every command in order, then print the final state.
  void run(Program &prog) {
    for (auto &cmd : prog)
      cmd->accept(*this); // dispatch to the visit() for the concrete command
    dump();
  }

  // selection : fold the item list with + (union) / - (difference).
  //   1 thru 10 + 12 - 1  ->  {1..10} ∪ {12} \ {1}  =  {2..10, 12}
  std::set<int> foldSelection(std::vector<Item> &items) {
    Resolver r(ctx);
    std::set<int> result;
    for (Item &item : items) {
      std::set<int> part = r.resolve(*item.sel);
      if (item.op == "-")
        for (int id : part)
          result.erase(id);
      else // "" (first) and "+" both add
        result.insert(part.begin(), part.end());
    }
    return result;
  }

  // Apply an 'at' to the current selection.
  void applyAt(const AtValue &at) {
    if (at.isPreset) {
      // Preset -> level mapping isn't modelled; just report the recall.
      std::cout << "  at preset (recall) on " << fmt(ctx.selected) << "\n";
      return;
    }
    for (int id : ctx.selected)
      if (id >= 1 && id <= Context::MAX_FIXTURES)
        ctx.level[id] = static_cast<float>(at.level);
    std::cout << "  at " << at.level << " -> " << fmt(ctx.selected) << "\n";
  }

  void visit(SelectCmd &c) override {
    ctx.selected = foldSelection(c.items);
    std::cout << "select " << fmt(ctx.selected) << "\n";
    if (c.hasAt) // "5 thru 12 at 100" — the at rides on the selection line
      applyAt(c.at);
  }

  void visit(StoreCmd &c) override {
    if (!ctx.hasSelection()) {
      std::cout << "store: nothing selected — ignored\n";
      return;
    }
    // The store target is specifically a Group or Preset (grammar: modsel), so
    // pull its key out directly rather than resolving it to fixtures.
    if (auto *g = dynamic_cast<Group *>(c.target.get())) {
      ctx.groups[static_cast<int>(g->id)] = ctx.selected;
      std::cout << "store group " << g->id << " <- " << fmt(ctx.selected) << "\n";
    } else if (auto *p = dynamic_cast<Preset *>(c.target.get())) {
      ctx.presets[{static_cast<int>(p->bank), static_cast<int>(p->number)}] = ctx.selected;
      std::cout << "store preset " << p->bank << "." << p->number << " <- "
                << fmt(ctx.selected) << "\n";
    }
  }

  void visit(DeleteCmd &c) override {
    if (!ctx.hasSelection()) {
      std::cout << "delete: nothing selected — ignored\n";
      return;
    }
    if (auto *g = dynamic_cast<Group *>(c.target.get())) {
      ctx.groups.erase(static_cast<int>(g->id));
      std::cout << "delete group " << g->id << "\n";
    } else if (auto *p = dynamic_cast<Preset *>(c.target.get())) {
      ctx.presets.erase({static_cast<int>(p->bank), static_cast<int>(p->number)});
      std::cout << "delete preset " << p->bank << "." << p->number << "\n";
    }
  }

  void visit(ClearCmd &) override {
    ctx.selected.clear();
    std::cout << "clear\n";
  }

  void visit(AtCmd &c) override {
    if (!ctx.hasSelection()) {
      std::cout << "at: nothing selected — ignored\n"; // e.g. 'at 100' after clear
      return;
    }
    applyAt(c.at);
  }

  // Final state dump.
  void dump() const {
    std::cout << "\n=== final state ===\n";
    std::cout << "selection: " << fmt(ctx.selected) << "\n";
    std::cout << "groups:\n";
    for (const auto &[id, fx] : ctx.groups)
      std::cout << "  group " << id << " = " << fmt(fx) << "\n";
    std::cout << "presets:\n";
    for (const auto &[key, fx] : ctx.presets)
      std::cout << "  preset " << key.first << "." << key.second << " = " << fmt(fx) << "\n";
    std::cout << "levels:\n";
    for (int i = 1; i <= Context::MAX_FIXTURES; ++i)
      if (ctx.level[i] != 0.0f)
        std::cout << "  fixture " << i << " @ " << ctx.level[i] << "\n";
  }
};
} // namespace Sel
