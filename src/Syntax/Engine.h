#pragma once
#include <optional>
#include <vector>

#include "Syntax/Grammar.h"
#include "Syntax/Node.h"
#include "../../include/SyntaxParser/Tokenizer/Parser.h"

namespace Parsing::Syntax
{
    // A backtracking recursive-descent engine that interprets a Grammar over a
    // flat token stream (this is essentially a PEG matcher). Ignored tokens
    // (e.g. WHITESPACE) must be filtered out before they reach the engine.
    class Engine
    {
        const Grammar &m_grammar;
        std::vector<Tokenizer::Token> m_tokens;
        std::size_t m_cur = 0;       // current token index
        std::size_t m_furthest = 0;  // furthest index reached, for error reporting

    public:
        Engine(const Grammar &grammar, std::vector<Tokenizer::Token> tokens);

        // Parse starting from `startRule`. Succeeds only if the rule matches
        // AND every token is consumed. Returns nullopt on failure.
        std::optional<Node> parse(const std::string &startRule);

        // Token index of the furthest point reached (where a failed parse
        // most likely went wrong).
        std::size_t furthestPos() const { return m_furthest; }
        const Tokenizer::Token *furthestToken() const;

    private:
        // Try to match `s`, appending any matched children to `out.kids`.
        // On failure, restores the cursor and trims kids back (full backtrack).
        bool match(const Symbol &s, Node &out);
    };
}
