#include "Syntax/Grammar.h"

namespace Parsing::Syntax
{
    namespace
    {
        // Return the first RuleRef in `s` that is not declared in `rules`,
        // or "" if all references resolve.
        std::string firstBadRef(const Symbol &s,
                                 const std::unordered_map<std::string, Rule> &rules)
        {
            if (s.kind == SymKind::RuleRef)
                return rules.contains(s.text) ? "" : s.text;

            for (const auto &c : s.children)
                if (std::string bad = firstBadRef(c, rules); !bad.empty())
                    return bad;
            return "";
        }
    }

    std::string Grammar::validate() const
    {
        if (startRule.empty() || !rules.contains(startRule))
            return startRule.empty() ? "<no rules>" : startRule;

        for (const auto &[name, rule] : rules)
            if (std::string bad = firstBadRef(rule.body, rules); !bad.empty())
                return bad;

        return "";
    }
}
