#include "Resolver.h"

namespace Sel {
std::set<int> Resolver::resolve(Selector &s) {
  ids.clear();
  s.accept(*this);
  return ids;
}

void Resolver::visit(Fixture &f) { ids = {static_cast<int>(f.id)}; }

void Resolver::visit(FixtureRange &r) {
  for (int i = static_cast<int>(r.from); i <= static_cast<int>(r.to); ++i)
    ids.insert(i);
}

void Resolver::visit(Group &g) {
  if (auto it = ctx.groups.find(static_cast<int>(g.id)); it != ctx.groups.end())
    ids = it->second;
}

void Resolver::visit(Preset &p) {
  auto key = std::pair{static_cast<int>(p.bank), static_cast<int>(p.number)};
  if (auto it = ctx.presets.find(key); it != ctx.presets.end())
    ids = it->second;
}
} // namespace Sel
