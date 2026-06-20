#pragma once
#include "Syntax/Visitor.h"

namespace Parsing::Syntax
{
    // Walks the parse tree of the arithmetic grammar and computes its value.
    //
    // Grammar it understands:
    //     expr   : term ((PLUS | MINUS) term)* ;
    //     term   : factor ((MUL | DIV) factor)* ;
    //     factor : NUM | LPAREN expr RPAREN ;
    //
    // The cascade (expr -> term -> factor) already encodes precedence, so the
    // evaluator just folds each level left-to-right (left-associative) and
    // reads the operator's *value* to pick the operation.
    class Evaluator : public Visitor<double>
    {
    protected:
        double visitTerminal(const Node &node) override;
        double visitRule(const Node &node) override;

    private:
        // expr / term share the shape: operand (OP operand)*  -> fold left.
        double foldBinary(const Node &node);
    };
}
