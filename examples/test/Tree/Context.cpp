#include "Context.h"

namespace Sel {
std::string fmt(const std::set<int> &s) {
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
} // namespace Sel
