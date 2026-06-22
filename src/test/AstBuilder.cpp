// =============================================================================
//  EXPLANATION / EXAMPLE CODE — NOT part of the build (lives outside src/).
// =============================================================================
//
//  One build* method per lang.syn rule. Each reads the CST node's `kids` BY
//  POSITION (layout follows from how the Engine flattens the grammar) and packs
//  them into typed AST nodes. Reminder on flattening:
//    Terminal/Literal -> leaf;  RuleRef -> child node;  Seq/Star/Optional/( )
//    flatten into the parent;  Choice -> only the winning alternative.

#include "AstBuilder.h"

#include <stdexcept>
#include <string>

namespace TempLang
{
    // entry : decl+        kids = [decl, ...]
    Program AstBuilder::buildProgram(const Node &entry)
    {
        Program program;
        for (const Node &decl : entry.kids)
            program.push_back(buildDecl(decl));
        return program;
    }

    // decl : fun_decl | struct_decl | var_decl     kids = [ the chosen rule node ]
    DeclPtr AstBuilder::buildDecl(const Node &decl)
    {
        const Node &c = decl.kids[0];
        if (c.rule == "var_decl")
            return buildVarDecl(c);
        if (c.rule == "fun_decl")
            return buildFunDecl(c);
        if (c.rule == "struct_decl")
            return buildStructDecl(c);
        throw std::runtime_error("buildDecl: unexpected '" + c.rule + "'");
    }

    // struct_decl : STRUCT IDENTIFIER LCURLY (var_decl)* RCURLY SEMIC
    //   kids = [STRUCT, IDENTIFIER, LCURLY, var_decl..., RCURLY, SEMIC]
    DeclPtr AstBuilder::buildStructDecl(const Node &s)
    {
        auto d = std::make_unique<StructDecl>();
        d->name = s.kids[1].token->value; // the IDENTIFIER after STRUCT
        for (const Node &k : s.kids)
            if (k.rule == "var_decl")
                d->fields.push_back(buildVarDecl(k));
        return d;
    }

    // var_decl : type IDENTIFIER (EQ expr)? SEMIC
    //   kids = [type, IDENTIFIER, EQ, expr, SEMIC] | [type, IDENTIFIER, SEMIC]
    DeclPtr AstBuilder::buildVarDecl(const Node &v)
    {
        auto d = std::make_unique<VarDecl>();
        d->type = buildType(v.kids[0]);
        d->name = v.kids[1].token->value;
        if (v.kids.size() > 2 && v.kids[2].isTokenName("EQ"))
            d->init = buildExpr(v.kids[3]);
        return d;
    }

    // fun_decl : type IDENTIFIER LPAREN (type IDENTIFIER (COMMA type IDENTIFIER)*)?
    //            RPAREN comp_stmt SEMIC
    //   kids: [type, IDENTIFIER, LPAREN, <flat params...>, RPAREN, comp_stmt, SEMIC]
    DeclPtr AstBuilder::buildFunDecl(const Node &f)
    {
        auto d = std::make_unique<FunDecl>();
        d->returnType = buildType(f.kids[0]);
        d->name = f.kids[1].token->value;

        // Params are a flat run of (type IDENTIFIER) pairs (COMMA leaves ignored)
        // between LPAREN and the comp_stmt body.
        for (std::size_t i = 3; i < f.kids.size(); ++i)
        {
            const Node &k = f.kids[i];
            if (k.rule == "comp_stmt")
            {
                d->body = buildCompound(k);
                break;
            }
            if (k.rule == "type")
                d->params.push_back({buildType(k), f.kids[i + 1].token->value});
        }
        return d;
    }

    // type : ptr_type | primitive_type
    //   ptr_type : primitive_type MUL+   kids = [primitive_type, MUL, MUL, ...]
    TypePtr AstBuilder::buildType(const Node &type)
    {
        const Node &inner = type.kids[0];

        if (inner.rule == "primitive_type")
            return buildPrimitiveType(inner);

        if (inner.rule == "ptr_type")
        {
            // Wrap the base primitive type in one PointerType per MUL.
            TypePtr t = buildPrimitiveType(inner.kids[0]);
            for (std::size_t i = 1; i < inner.kids.size(); ++i)
            {
                auto p = std::make_unique<PointerType>();
                p->pointee = std::move(t);
                t = std::move(p);
            }
            return t;
        }

        throw std::runtime_error("buildType: unexpected '" + inner.rule + "'");
    }

    // primitive_type : atomic_type | named_type   kids = [ the chosen rule node ]
    TypePtr AstBuilder::buildPrimitiveType(const Node &prim)
    {
        const Node &inner = prim.kids[0];

        if (inner.rule == "atomic_type") // atomic_type : INT|CHAR|VOID|BOOL   kids = [ leaf ]
        {
            const std::string &tok = inner.kids[0].token->name;
            auto t = std::make_unique<AtomicType>();
            if (tok == "INT")
                t->prim = AtomicType::Prim::Int;
            else if (tok == "CHAR")
                t->prim = AtomicType::Prim::Char;
            else if (tok == "VOID")
                t->prim = AtomicType::Prim::Void;
            else if (tok == "BOOL")
                t->prim = AtomicType::Prim::Bool;
            else
                throw std::runtime_error("buildPrimitiveType: unknown atomic type '" + tok + "'");
            return t;
        }

        if (inner.rule == "named_type") // named_type : IDENTIFIER   kids = [ leaf ]
        {
            auto t = std::make_unique<NamedType>();
            t->name = inner.kids[0].token->value;
            return t;
        }

        throw std::runtime_error("buildPrimitiveType: unexpected '" + inner.rule + "'");
    }

    // stmt : var_decl | assign_stmt | expr_stmt | comp_stmt   kids = [chosen alt]
    StmtPtr AstBuilder::buildStmt(const Node &stmt)
    {
        const Node &c = stmt.kids[0];

        if (c.rule == "var_decl") // a local declaration used as a statement
        {
            auto s = std::make_unique<VarDeclStmt>();
            s->decl = buildVarDecl(c);
            return s;
        }
        if (c.rule == "assign_stmt")
            return buildAssign(c);
        if (c.rule == "return_stmt")
            return buildReturn(c);
        if (c.rule == "expr_stmt") // expr_stmt : expr SEMIC
        {
            auto s = std::make_unique<ExprStmt>();
            s->expr = buildExpr(c.kids[0]);
            return s;
        }
        if (c.rule == "comp_stmt")
            return buildCompound(c);

        throw std::runtime_error("buildStmt: unexpected '" + c.rule + "'");
    }

    // assign_stmt : expr EQ expr SEMIC    kids = [expr, EQ, expr, SEMIC]
    StmtPtr AstBuilder::buildAssign(const Node &a)
    {
        auto s = std::make_unique<AssignStmt>();
        s->e1 = buildExpr(a.kids[0]); // target
        s->e2 = buildExpr(a.kids[2]); // value
        return s;
    }

    // return_stmt : RETURN expr? SEMIC
    //   kids = [RETURN, expr, SEMIC]  or  [RETURN, SEMIC]  (bare return)
    StmtPtr AstBuilder::buildReturn(const Node &ret)
    {
        auto s = std::make_unique<ReturnStmt>();
        if (ret.kids[1].rule == "expr") // optional value present
            s->ret = buildExpr(ret.kids[1]);
        return s;
    }

    // comp_stmt : LCURLY stmt* RCURLY   -> collect the stmt kids
    StmtPtr AstBuilder::buildCompound(const Node &comp)
    {
        auto s = std::make_unique<CompoundStmt>();
        for (const Node &k : comp.kids)
            if (k.rule == "stmt")
                s->body.push_back(buildStmt(k));
        return s;
    }

    // The expression cascade, collapsed into Number/Binary/Unary/Call nodes.
    ExprPtr AstBuilder::buildExpr(const Node &node)
    {
        if (node.rule == "expr" || node.rule == "plusexpr") // head tail?
        {
            ExprPtr head = buildExpr(node.kids[0]);
            return node.kids.size() == 2 ? foldTail(std::move(head), node.kids[1])
                                         : std::move(head);
        }

        if (node.rule == "mulexpr") // mulexpr : access_expr  (single child)
            return buildExpr(node.kids[0]);

        if (node.rule == "access_expr") // angexpr (DOT IDENTIFIER)*
        {
            ExprPtr base = buildExpr(node.kids[0]); // the angexpr
            // Fold each `.member` into a MemberExpr, left-associative:
            //   p.x.y -> Member(Member(p, x), y)
            for (std::size_t i = 1; i < node.kids.size(); ++i)
                if (node.kids[i].isTokenName("IDENTIFIER"))
                {
                    auto m = std::make_unique<MemberExpr>();
                    m->base = std::move(base);
                    m->member = node.kids[i].token->value;
                    base = std::move(m);
                }
            return base;
        }

        if (node.rule == "angexpr") // NUM | group | funcall | IDENTIFIER
        {
            const Node &k = node.kids[0];
            if (k.isTokenName("NUM"))
            {
                auto n = std::make_unique<NumberExpr>();
                n->value = std::stoll(k.token->value);
                return n;
            }

            if (k.isTokenName("IDENTIFIER"))
            {
                auto n = std::make_unique<NamedExpr>();
                n->value = k.token->value;
                return n;
            }
            return buildExpr(k); // group or funcall node
        }

        if (node.rule == "group") // ( expr )
            return buildExpr(node.kids[1]);

        if (node.rule == "funcall") // IDENTIFIER ( expr (, expr)* )
        {
            auto call = std::make_unique<CallExpr>();
            call->callee = node.kids[0].token->value;
            for (std::size_t i = 2; i < node.kids.size(); ++i)
                if (node.kids[i].rule == "expr")
                    call->args.push_back(buildExpr(node.kids[i]));
            return call;
        }

        throw std::runtime_error("buildExpr: unhandled rule '" + node.rule + "'");
    }

    // (PLUS|MINUS|MUL|DIV) operand tail?  -> left-associative BinaryExpr chain
    ExprPtr AstBuilder::foldTail(ExprPtr lhs, const Node &tail)
    {
        auto bin = std::make_unique<BinaryExpr>();
        bin->op = tail.kids[0].token->value;
        bin->lhs = std::move(lhs);
        bin->rhs = buildExpr(tail.kids[1]);
        return tail.kids.size() == 3 ? foldTail(std::move(bin), tail.kids[2])
                                     : std::move(bin);
    }
}
