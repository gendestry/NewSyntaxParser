#include "Syntax/Grammar.h"

#include <iostream>
#include <vector>

#include "Utils/Colors/Font.h"
#include "Utils/Text/Stream.h"

namespace Parsing::Syntax
{
    namespace
    {
        // Wrap `inner` in parens unless it's already a single atom / postfix op.
        std::string group(const Symbol &s, const std::string &inner)
        {
            const bool atomic = s.kind == SymKind::Terminal || s.kind == SymKind::Literal ||
                                s.kind == SymKind::RuleRef || s.kind == SymKind::Star ||
                                s.kind == SymKind::Plus || s.kind == SymKind::Optional;
            return atomic ? inner : "(" + inner + ")";
        }

        std::string join(const std::vector<Symbol> &kids, const std::string &sep)
        {
            std::string out;
            for (std::size_t i = 0; i < kids.size(); ++i)
                out += (i ? sep : "") + toEbnf(kids[i]);
            return out;
        }

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

    std::string toEbnf(const Symbol &sym)
    {
        switch (sym.kind)
        {
            case SymKind::Terminal: return sym.text;
            case SymKind::RuleRef:  return sym.text;
            case SymKind::Literal:  return "'" + sym.text + "'";
            case SymKind::Seq:      return join(sym.children, " ");
            case SymKind::Choice:   return join(sym.children, " | ");
            case SymKind::Star:     return group(sym.children[0], toEbnf(sym.children[0])) + "*";
            case SymKind::Plus:     return group(sym.children[0], toEbnf(sym.children[0])) + "+";
            case SymKind::Optional: return group(sym.children[0], toEbnf(sym.children[0])) + "?";
        }
        return "";
    }

    void printGrammar(const Grammar &grammar)
    {
        using namespace Utils::Font;

        auto one = [](const Rule &r)
        {
            Utils::Text::Stream s;
            s << colorMagenta << r.name << colorReset
              << colorDim << " : " << colorReset
              << toEbnf(r.body)
              << colorDim << " ;" << colorReset;
            std::cout << s.end() << std::endl;
        };

        if (auto it = grammar.rules.find(grammar.startRule); it != grammar.rules.end())
            one(it->second);
        for (const auto &[name, rule] : grammar.rules)
            if (name != grammar.startRule)
                one(rule);
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
