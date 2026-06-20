#include "Syntax/Evaluator.h"

#include <stdexcept>
#include <string>

namespace Parsing::Syntax
{
    double Evaluator::visitTerminal(const Node &node)
    {
        // The only operand leaf is NUM; operator leaves are read directly in
        // foldBinary() and never visited as values.
        if (node.isTokenName("NUM"))
            return std::stod(node.token->value);

        throw std::runtime_error("evaluator: unexpected terminal '" +
                                 node.token->name + "'");
    }

    double Evaluator::visitRule(const Node &node)
    {
        if (node.rule == "expr" || node.rule == "term")
            return foldBinary(node);

        if (node.rule == "factor")
        {
            // factor : NUM | LPAREN expr RPAREN
            if (node.kids.size() == 1)          // NUM
                return visit(node.kids[0]);
            return visit(node.kids[1]);         // ( expr ) -> the inner expr
        }

        // Any other single-child wrapper rule: pass through.
        if (node.kids.size() == 1)
            return visitOnly(node);

        throw std::runtime_error("evaluator: don't know how to evaluate rule '" +
                                 node.rule + "'");
    }

    double Evaluator::foldBinary(const Node &node)
    {
        // kids = [operand, OP, operand, OP, operand, ...]
        double acc = visit(node.kids[0]);
        for (std::size_t i = 1; i + 1 < node.kids.size(); i += 2)
        {
            const std::string &op = node.kids[i].token->value; // '+', '-', '*', '/'
            double rhs = visit(node.kids[i + 1]);

            if (op == "+") acc += rhs;
            else if (op == "-") acc -= rhs;
            else if (op == "*") acc *= rhs;
            else if (op == "/") acc /= rhs;
            else
                throw std::runtime_error("evaluator: unknown operator '" + op + "'");
        }
        return acc;
    }
}
