#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace Parsing::Syntax
{
    // A Symbol is one node in the right-hand side of a grammar rule.
    // Terminals/Literals/RuleRefs are leaves; the rest are combinators
    // whose `children` hold the sub-expressions.
    enum class SymKind
    {
        Terminal, // matches a token by NAME      (e.g. NUM, PLUS)  -> text = token name
        Literal,  // matches a token by VALUE      (e.g. '+')        -> text = literal value
        RuleRef,  // matches another rule          (e.g. expr)       -> text = rule name
        Seq,      // a b c        : all children, in order
        Choice,   // a | b | c    : first child that matches
        Star,     // a*           : zero or more of children[0]
        Plus,     // a+           : one or more of children[0]
        Optional, // a?           : zero or one of children[0]
    };

    struct Symbol
    {
        SymKind kind;
        std::string text;             // name/literal for the leaf kinds
        std::vector<Symbol> children; // sub-expressions for the combinators
    };

    struct Rule
    {
        std::string name;
        Symbol body;
    };

    struct Grammar
    {
        std::unordered_map<std::string, Rule> rules;
        std::string startRule; // first rule declared in the file

        bool has(const std::string &name) const { return rules.contains(name); }

        // Verify every RuleRef points at a declared rule and the start rule
        // exists. Returns the offending name on failure, "" on success.
        std::string validate() const;
    };

    // --- Symbol construction helpers (used by the grammar parser) ---------
    inline Symbol terminal(std::string name) { return {SymKind::Terminal, std::move(name), {}}; }
    inline Symbol literal(std::string value) { return {SymKind::Literal, std::move(value), {}}; }
    inline Symbol ruleRef(std::string name) { return {SymKind::RuleRef, std::move(name), {}}; }
    inline Symbol seq(std::vector<Symbol> c) { return {SymKind::Seq, "", std::move(c)}; }
    inline Symbol choice(std::vector<Symbol> c) { return {SymKind::Choice, "", std::move(c)}; }
    inline Symbol star(Symbol c) { return {SymKind::Star, "", {std::move(c)}}; }
    inline Symbol plus(Symbol c) { return {SymKind::Plus, "", {std::move(c)}}; }
    inline Symbol optional(Symbol c) { return {SymKind::Optional, "", {std::move(c)}}; }
}
