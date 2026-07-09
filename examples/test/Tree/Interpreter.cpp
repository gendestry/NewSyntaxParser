#include "Interpreter.h"

#include "Resolver.h"

#include <iostream>

namespace Sel {

void Interpreter::execute(Program &prog) {
  for (auto &cmd : prog)
    cmd->accept(*this);
}

void Interpreter::run(Program &prog) {
  execute(prog);
  dump();
}

std::set<int> Interpreter::foldSelection(std::vector<Item> &items) {
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

void Interpreter::applyAt(const AtValue &at) {
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

void Interpreter::visit(SelectCmd &c) {
  ctx.selected = foldSelection(c.items);
  std::cout << "select " << fmt(ctx.selected) << "\n";
  if (c.hasAt)
    applyAt(c.at);
}

void Interpreter::visit(StoreCmd &c) {
  if (!ctx.hasSelection()) {
    std::cout << "store: nothing selected — ignored\n";
    return;
  }
  if (auto *g = dynamic_cast<Group *>(c.target.get())) {
    ctx.groups[static_cast<int>(g->id)] = ctx.selected;
    std::cout << "store group " << g->id << " <- " << fmt(ctx.selected) << "\n";
  } else if (auto *p = dynamic_cast<Preset *>(c.target.get())) {
    ctx.presets[{static_cast<int>(p->bank), static_cast<int>(p->number)}] = ctx.selected;
    std::cout << "store preset " << p->bank << "." << p->number << " <- "
              << fmt(ctx.selected) << "\n";
  }
}

void Interpreter::visit(DeleteCmd &c) {
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

void Interpreter::visit(ClearCmd &) {
  ctx.selected.clear();
  std::cout << "clear\n";
}

void Interpreter::visit(AtCmd &c) {
  if (!ctx.hasSelection()) {
    std::cout << "at: nothing selected — ignored\n";
    return;
  }
  applyAt(c.at);
}

void Interpreter::dump() const {
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
} // namespace Sel
