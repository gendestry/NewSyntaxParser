#pragma once
// Interpreter state: the current selection plus stored groups / presets and
// per-fixture levels. Owned by the Interpreter; mutated as commands execute.

#include <array>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace Sel {
struct Context {
  static constexpr int MAX_FIXTURES = 20;

  std::set<int> selected;                               // active fixture ids
  std::map<int, std::set<int>> groups;                  // group id  -> fixtures
  std::map<std::pair<int, int>, std::set<int>> presets; // (bank,num) -> fixtures
  std::array<float, MAX_FIXTURES + 1> level{};          // fixture 1..20 -> level

  bool hasSelection() const { return !selected.empty(); }
};

// Render a set of ids as {a, b, c}.
std::string fmt(const std::set<int> &s);
} // namespace Sel
