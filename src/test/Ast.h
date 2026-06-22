#pragma once
// =============================================================================
//  EXPLANATION / EXAMPLE CODE — NOT part of the build (lives outside src/).
// =============================================================================
//
//  Typed AST for the FULL lang.syn (functions, pointers, sin/cos, calls,
//  compound statements). Counterpart of temp/Ast.h (which targets the smaller
//  explain.syn). Kept in namespace TempLang so the two examples never clash.
//
//      Type  -> AtomicType | PointerType
//      Expr  -> NumberExpr | BinaryExpr | UnaryExpr | CallExpr
//      Stmt  -> ExprStmt | CompoundStmt
//      Decl  -> VarDecl | FunDecl
//      Program = vector<Decl>
//
//  Data only; a tiny virtual print() lets you see the tree without evaluating.

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace TempLang
{
    inline std::string pad(int n) { return std::string(n, ' '); }

    // ---- TYPE ----------------------------------------------------------------
    struct Type
    {
        virtual ~Type() = default;
        virtual std::string toString() const = 0;
    };
    using TypePtr = std::unique_ptr<Type>;

    // atomic_type : INT | CHAR | VOID | BOOL
    struct AtomicType : Type
    {
        enum class Prim
        {
            Int,
            Char,
            Void,
            Bool
        } prim = Prim::Int;
        std::string toString() const override
        {
            switch (prim)
            {
            case Prim::Int:
                return "int";
            case Prim::Char:
                return "char";
            case Prim::Void:
                return "void";
            case Prim::Bool:
                return "bool";
            }
            return "?";
        }
    };

    // ptr_type : atomic_type MUL+   (nested: int** = Pointer(Pointer(int)))
    struct PointerType : Type
    {
        TypePtr pointee;
        std::string toString() const override { return pointee->toString() + "*"; }
    };

    struct NamedType : Type
    {
        std::string name;
        std::string toString() const override { return name; }
    };

    // ---- EXPR ----------------------------------------------------------------
    struct Expr
    {
        virtual ~Expr() = default;
        virtual void print(int indent = 0) const = 0;
    };
    using ExprPtr = std::unique_ptr<Expr>;

    struct NumberExpr : Expr // NUM
    {
        long long value = 0;
        void print(int i) const override { std::cout << pad(i) << "Number " << value << "\n"; }
    };

    struct NamedExpr : Expr // NUM
    {
        std::string value;
        void print(int i) const override { std::cout << pad(i) << "NamedExpr " << value << "\n"; }
    };

    struct BinaryExpr : Expr // + - * /
    {
        std::string op;
        ExprPtr lhs, rhs;
        void print(int i) const override
        {
            std::cout << pad(i) << "Binary '" << op << "'\n";
            lhs->print(i + 2);
            rhs->print(i + 2);
        }
    };

    struct UnaryExpr : Expr // (SIN | COS) operand
    {
        std::string op;
        ExprPtr operand;
        void print(int i) const override
        {
            std::cout << pad(i) << "Unary '" << op << "'\n";
            operand->print(i + 2);
        }
    };

    struct CallExpr : Expr // funcall : IDENTIFIER ( args )
    {
        std::string callee;
        std::vector<ExprPtr> args;
        void print(int i) const override
        {
            std::cout << pad(i) << "Call " << callee << "(" << args.size() << " arg)\n";
            for (auto &a : args)
                a->print(i + 2);
        }
    };

    struct MemberExpr : Expr // access_expr: base.member  (e.g. p.x)
    {
        ExprPtr base;
        std::string member;
        void print(int i) const override
        {
            std::cout << pad(i) << "Member ." << member << "\n";
            base->print(i + 2);
        }
    };

    // ---- STMT ----------------------------------------------------------------
    struct Stmt
    {
        virtual ~Stmt() = default;
        virtual void print(int indent = 0) const = 0;
    };
    using StmtPtr = std::unique_ptr<Stmt>;

    struct ExprStmt : Stmt
    {
        ExprPtr expr;
        void print(int i) const override
        {
            std::cout << pad(i) << "ExprStmt\n";
            expr->print(i + 2);
        }
    };

    struct CompoundStmt : Stmt // comp_stmt : { (stmt ;)* }
    {
        std::vector<StmtPtr> body;
        void print(int i) const override
        {
            std::cout << pad(i) << "Compound {\n";
            for (auto &s : body)
                s->print(i + 2);
            std::cout << pad(i) << "}\n";
        }
    };

    struct AssignStmt : Stmt
    {
        ExprPtr e1;
        ExprPtr e2;

        void print(int i) const override
        {
            std::cout << pad(i) << "AssignStmt\n";
            e1->print(i + 2);
            e2->print(i + 2);
        }
    };

    struct ReturnStmt : Stmt
    {
        ExprPtr ret;

        void print(int i) const override
        {
            std::cout << pad(i) << "ReturnStmt\n";
            if (ret)
                ret->print(i + 2);
        }
    };

    // ---- DECL ----------------------------------------------------------------
    struct Decl
    {
        virtual ~Decl() = default;
        virtual void print(int indent = 0) const = 0;
    };
    using DeclPtr = std::unique_ptr<Decl>;

    struct VarDecl : Decl // var_decl : type IDENTIFIER (= expr)? ;
    {
        TypePtr type;
        std::string name;
        ExprPtr init; // null when uninitialised
        void print(int i) const override
        {
            std::cout << pad(i) << "VarDecl " << type->toString() << " " << name
                      << (init ? " =" : " (uninitialised)") << "\n";
            if (init)
                init->print(i + 2);
        }
    };

    // struct_decl : STRUCT IDENTIFIER LCURLY (var_decl)* RCURLY SEMIC
    struct StructDecl : Decl
    {
        std::string name;
        std::vector<DeclPtr> fields; // each one is a VarDecl
        void print(int i) const override
        {
            std::cout << pad(i) << "StructDecl " << name << " {\n";
            for (const auto &f : fields)
                f->print(i + 2);
            std::cout << pad(i) << "}\n";
        }
    };

    struct FunDecl : Decl // fun_decl : type IDENTIFIER ( params ) comp_stmt ;
    {
        struct Param
        {
            TypePtr type;
            std::string name;
        };
        TypePtr returnType;
        std::string name;
        std::vector<Param> params;
        StmtPtr body; // a CompoundStmt
        void print(int i) const override
        {
            std::cout << pad(i) << "FunDecl " << returnType->toString() << " " << name << "(";
            for (std::size_t k = 0; k < params.size(); ++k)
                std::cout << (k ? ", " : "") << params[k].type->toString() << " " << params[k].name;
            std::cout << ")\n";
            if (body)
                body->print(i + 2);
        }
    };

    // A var_decl used as a statement (stmt : var_decl | ...). Wraps the VarDecl
    // so local declarations can appear inside a compound statement. Defined here
    // (after the Decl types) because it holds a DeclPtr.
    struct VarDeclStmt : Stmt
    {
        DeclPtr decl; // a VarDecl
        void print(int i) const override
        {
            std::cout << pad(i) << "VarDeclStmt\n";
            decl->print(i + 2);
        }
    };

    using Program = std::vector<DeclPtr>;
}
