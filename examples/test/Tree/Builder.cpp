#include "Builder.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace Sel {
using Node = Parsing::Syntax::Node;

// A NUM leaf may carry a decimal ('.' [0-9]+)?; parse once, use as int or real.
static double numOf(const Node &leaf) { return std::stod(leaf.token->value); }
static long long intOf(const Node &leaf) {
  return static_cast<long long>(numOf(leaf));
}

// grpsel : GROUP NUM              kids = [GROUP, NUM]
static std::unique_ptr<Group> buildGrpsel(const Node &g) {
  auto n = std::make_unique<Group>();
  n->id = intOf(g.kids[1]);
  return n;
}

// presetsel : PRESET NUM DOT NUM  kids = [PRESET, NUM, DOT, NUM]
static std::unique_ptr<Preset> buildPresetsel(const Node &p) {
  auto n = std::make_unique<Preset>();
  n->bank = intOf(p.kids[1]);
  n->number = intOf(p.kids[3]);
  return n;
}

// fixsel : NUM (THRU NUM)?        kids = [NUM] | [NUM, THRU, NUM]
static SelectorPtr buildFixsel(const Node &f) {
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
static SelectorPtr buildSel(const Node &sel) {
  const Node &c = sel.kids[0];
  if (c.rule == "fixsel")
    return buildFixsel(c);
  if (c.rule == "grpsel")
    return buildGrpsel(c);
  throw std::runtime_error("buildSel: unexpected '" + c.rule + "'");
}

// modsel : grpsel | presetsel     kids = [ chosen ]
static SelectorPtr buildModsel(const Node &m) {
  const Node &c = m.kids[0];
  if (c.rule == "grpsel")
    return buildGrpsel(c);
  if (c.rule == "presetsel")
    return buildPresetsel(c);
  throw std::runtime_error("buildModsel: unexpected '" + c.rule + "'");
}

// at : AT (NUM | presetsel)       kids = [AT, NUM | presetsel]
static AtValue buildAt(const Node &at) {
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
static void buildItems(const Node &selection, std::vector<Item> &out) {
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
static CommandPtr buildCommand(const Node &command) {
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
Program buildProgram(const Node &entry) {
  Program prog;
  for (const Node &cmd : entry.kids)
    if (cmd.rule == "command")
      prog.push_back(buildCommand(cmd));
  return prog;
}
} // namespace Sel
