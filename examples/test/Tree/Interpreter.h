#pragma once
// Command interpreter: walks the program, mutating a Context. Enforces the
// "requires a selection" guards (store / delete / at do nothing when nothing
// is selected; clear is always allowed).

#include "Ast.gen.h"
#include "Context.h"

#include <set>
#include <vector>

namespace Sel {
struct Interpreter : CommandVisitor {
  Context ctx;

  // Execute every command in order (no output beyond per-command logging).
  void execute(Program &prog);

  // execute() followed by a full state dump — for one-shot / batch runs.
  void run(Program &prog);

  void visit(SelectCmd &c) override;
  void visit(StoreCmd &c) override;
  void visit(DeleteCmd &c) override;
  void visit(ClearCmd &) override;
  void visit(AtCmd &c) override;

  void dump() const;

private:
  // selection : fold the item list with + (union) / - (difference).
  std::set<int> foldSelection(std::vector<Item> &items);
  // Apply an 'at' to the current selection.
  void applyAt(const AtValue &at);
};
} // namespace Sel
