#pragma once
#include <optional>
#include <string>
#include <vector>

#include "SyntaxParser/Tokenizer/Parser.h"

namespace Parsing::Syntax
{
    // A homogeneous parse-tree node (like ANTLR's runtime ParseTree).
    //  - A leaf holds a Token   -> its value/col/row are preserved verbatim.
    //  - An internal node holds the rule name and its children.
    struct Node
    {
        std::string rule;                            // rule name; empty on leaves
        std::optional<Tokenizer::Token> token;       // set on leaves
        std::vector<Node> kids;

        bool isLeaf() const { return token.has_value(); }

        // Display text: the token's value for leaves, the rule name otherwise.
        std::string text() const { return token ? token->value : rule; }

        // Convenience: does this leaf carry a token with the given name?
        bool isTokenName(const std::string &name) const
        {
            return token && token->name == name;
        }
    };

    // Pretty-print the whole tree, indented. Not the visitor pattern itself —
    // just a debugging dump that shows what the engine produced.
    void printTree(const Node &node, int depth = 0);
}
