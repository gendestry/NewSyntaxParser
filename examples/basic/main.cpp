// =============================================================================
//  Basic arithmetic example for the synparser library.
//
//  Pipeline:
//    1. Lex  input.txt using the token rules in tokens.txt.
//    2. Parse the token stream against lang.syn into a homogeneous parse tree.
//    3. Lower that tree into the typed AST generated from ast.spec (Basic::).
//    4. Walk the AST with a visitor to compute the value.
// =============================================================================
#include "Tokenizer/Parser.h"
#include "Syntax/GrammarParser.h"
#include "Syntax/Engine.h"

#include "Ast.gen.h" // generated from ast.spec: Basic:: NumberExpr / BinaryExpr

#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using Parsing::Syntax::Node;

// ---- CST -> typed AST -------------------------------------------------------
static Basic::ExprPtr buildExpr(const Node &node);

// tail : (OP operand) tail?   -> fold into a left-associative BinaryExpr chain.
static Basic::ExprPtr foldTail(Basic::ExprPtr lhs, const Node &tail)
{
    auto bin = std::make_unique<Basic::BinaryExpr>();
    bin->op = tail.kids[0].token->value; // the PLUS/MINUS/MUL/DIV leaf
    bin->lhs = std::move(lhs);
    bin->rhs = buildExpr(tail.kids[1]);
    return tail.kids.size() == 3 ? foldTail(std::move(bin), tail.kids[2])
                                 : std::move(bin);
}

static Basic::ExprPtr buildExpr(const Node &node)
{
    // expr / plusexpr : head tail?
    if (node.rule == "expr" || node.rule == "plusexpr")
    {
        Basic::ExprPtr head = buildExpr(node.kids[0]);
        return node.kids.size() == 2 ? foldTail(std::move(head), node.kids[1])
                                     : std::move(head);
    }

    if (node.rule == "mulexpr") // mulexpr : atom  (pass-through)
        return buildExpr(node.kids[0]);

    if (node.rule == "atom") // atom : NUM | group
    {
        const Node &k = node.kids[0];
        if (k.isTokenName("NUM"))
        {
            auto num = std::make_unique<Basic::NumberExpr>();
            num->value = std::stoll(k.token->value);
            return num;
        }
        return buildExpr(k); // group
    }

    if (node.rule == "group") // group : ( expr )
        return buildExpr(node.kids[1]);

    throw std::runtime_error("buildExpr: unhandled rule '" + node.rule + "'");
}

// ---- AST evaluator (visitor over the generated Basic:: nodes) --------------
struct Evaluator : Basic::ExprVisitor
{
    long long result = 0;

    long long eval(Basic::Expr &e)
    {
        e.accept(*this);
        return result;
    }

    void visit(Basic::NumberExpr &n) override { result = n.value; }

    void visit(Basic::BinaryExpr &b) override
    {
        long long lhs = eval(*b.lhs);
        long long rhs = eval(*b.rhs);
        if (b.op == "+") result = lhs + rhs;
        else if (b.op == "-") result = lhs - rhs;
        else if (b.op == "*") result = lhs * rhs;
        else if (b.op == "/") result = lhs / rhs;
        else throw std::runtime_error("unknown operator '" + b.op + "'");
    }
};

int main()
{
    // 1. Lex.
    Parsing::Tokenizer::Parser lexer("tokens.txt");
    lexer.parse("input.txt");

    std::vector<Parsing::Tokenizer::Token> tokens;
    for (auto &t : lexer.getTokens())
        if (!t.ignore)
            tokens.push_back(t);

    // 2. Parse against the grammar.
    auto grammar = Parsing::Syntax::GrammarParser::parseFile("lang.syn");
    Parsing::Syntax::Engine engine(grammar, tokens);
    auto cst = engine.parse(grammar.startRule);
    if (!cst)
    {
        if (const auto *t = engine.furthestToken())
            std::cerr << "parse error near " << t->name << " '" << t->value << "'\n";
        else
            std::cerr << "parse error: unexpected end of input\n";
        return 1;
    }

    // 3. Lower to the typed AST (entry : expr, so the expr is the first kid).
    Basic::ExprPtr ast = buildExpr(cst->kids[0]);

    // 4. Evaluate.
    Evaluator evaluator;
    std::cout << "result = " << evaluator.eval(*ast) << "\n";
    return 0;
}
