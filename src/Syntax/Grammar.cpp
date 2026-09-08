#include "Syntax/Grammar.h"

#include <iostream>
#include <vector>

#include "Utils/Colors/ColorFormatter.h"
#include "Utils/Colors/Font.h"
#include "Utils/Colors/Theme.h"

namespace Parsing::Syntax
{
    namespace
    {
        // A leaf or a postfix op binds tightly enough to stand without parens.
        bool isAtomic(const Symbol &s)
        {
            return s.kind == SymKind::Terminal || s.kind == SymKind::Literal ||
                   s.kind == SymKind::RuleRef || s.kind == SymKind::Star ||
                   s.kind == SymKind::Plus || s.kind == SymKind::Optional;
        }

        // Wrap `inner` in parens unless it's already a single atom / postfix op.
        std::string group(const Symbol &s, const std::string &inner)
        {
            return isAtomic(s) ? inner : "(" + inner + ")";
        }

        std::string join(const std::vector<Symbol> &kids, const std::string &sep)
        {
            std::string out;
            for (std::size_t i = 0; i < kids.size(); ++i)
                out += (i ? sep : "") + toEbnf(kids[i]);
            return out;
        }

        // --- Themed variants: same shape as toEbnf, one Theme colour per kind ---
        //
        // Kept separate from toEbnf so the plain version stays usable for
        // anything that wants EBNF text without escape codes in it.

        std::string toEbnfThemed(const Symbol &sym);

        // Punctuation the grammar author didn't write: parens, operators, ':' and ';'.
        std::string punct(std::string_view s) { return Theme::dim(s).str(); }

        std::string groupThemed(const Symbol &s, const std::string &inner)
        {
            return isAtomic(s) ? inner : punct("(") + inner + punct(")");
        }

        std::string joinThemed(const std::vector<Symbol> &kids, const std::string &sep)
        {
            std::string out;
            for (std::size_t i = 0; i < kids.size(); ++i)
                out += (i ? sep : "") + toEbnfThemed(kids[i]);
            return out;
        }

        std::string toEbnfThemed(const Symbol &sym)
        {
            switch (sym.kind)
            {
                case SymKind::Terminal: return Theme::lbl(sym.text).str();
                case SymKind::RuleRef:  return Theme::name(sym.text).str();
                case SymKind::Literal:  return Theme::lime("'" + sym.text + "'").str();
                case SymKind::Seq:      return joinThemed(sym.children, " ");
                case SymKind::Choice:   return joinThemed(sym.children, punct(" | "));
                case SymKind::Star:     return groupThemed(sym.children[0], toEbnfThemed(sym.children[0])) + Theme::accent("*").str();
                case SymKind::Plus:     return groupThemed(sym.children[0], toEbnfThemed(sym.children[0])) + Theme::accent("+").str();
                case SymKind::Optional: return groupThemed(sym.children[0], toEbnfThemed(sym.children[0])) + Theme::accent("?").str();
            }
            return "";
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

    std::string Grammar::validate() const
    {
        if (startRule.empty() || !rules.contains(startRule))
            return startRule.empty() ? "<no rules>" : startRule;

        for (const auto &[name, rule] : rules)
            if (std::string bad = firstBadRef(rule.body, rules); !bad.empty())
                return bad;

        return "";
    }

    std::string Grammar::toString() const
    {
        std::stringstream ss;
        auto one = [&](const Rule &r)
        {
            ss << Theme::name(r.name).str()
                      << punct(" : ")
                      << toEbnfThemed(r.body)
                      << punct(" ;") << "\n";
        };

        if (auto it = rules.find(startRule); it != rules.end())
            one(it->second);

        for (const auto &rulename : sortedRules)
        {
            if (rulename == startRule)
                continue;
            if (auto it = rules.find(rulename); it != rules.end())
                one(it->second);
        }
        return ss.str();
    }
}
