#pragma once
#include "Syntax/Node.h"

namespace Parsing::Syntax
{
    // Generic tree visitor returning a value of type R.
    //
    // Because the grammar is interpreted at runtime, nodes are homogeneous and
    // dispatch happens on the rule NAME (for internal nodes) or the token name
    // (for leaves) inside the derived class. This mirrors ANTLR's runtime
    // AbstractParseTreeVisitor<T>.
    //
    // Derived classes implement:
    //   - visitTerminal(): handle a leaf token (its value lives on node.token).
    //   - visitRule():     handle an internal node, switching on node.rule.
    // and call visit()/visitChildrenLast() to recurse.
    template <typename R>
    class Visitor
    {
    public:
        virtual ~Visitor() = default;

        R visit(const Node &node)
        {
            return node.isLeaf() ? visitTerminal(node) : visitRule(node);
        }

    protected:
        virtual R visitTerminal(const Node &node) = 0;
        virtual R visitRule(const Node &node) = 0;

        // Helper: visit the result of the last child (handy for single-child
        // "pass-through" wrapper rules).
        R visitOnly(const Node &node) { return visit(node.kids.back()); }
    };
}
