#pragma once
// Selector resolver: expands ONE selector (Fixture / FixtureRange / Group /
// Preset) into the concrete set of fixture ids it denotes. Groups and presets
// are looked up in the Context.

#include "Ast.gen.h"
#include "Context.h"

#include <set>

namespace Sel {
struct Resolver : SelectorVisitor {
  const Context &ctx;
  std::set<int> ids; // result of the last resolve()

  explicit Resolver(const Context &c) : ctx(c) {}

  std::set<int> resolve(Selector &s);

  void visit(Fixture &f) override;
  void visit(FixtureRange &r) override;
  void visit(Group &g) override;
  void visit(Preset &p) override;
};
} // namespace Sel
