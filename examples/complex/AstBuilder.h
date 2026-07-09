#pragma once
// =============================================================================
//  EXPLANATION / EXAMPLE CODE — NOT part of the build (lives outside src/).
// =============================================================================
//
//  CST -> AST lowering for the FULL lang.syn. Walks the homogeneous Node tree
//  and constructs typed TempLang:: nodes. No evaluation.

#include "Ast.h"
#include "Syntax/Node.h" // Parsing::Syntax::Node

namespace TempLang
{
    class AstBuilder
    {
    public:
        Program buildProgram(const Parsing::Syntax::Node &entry); // entry : decl+

    private:
        using Node = Parsing::Syntax::Node;

        DeclPtr buildDecl(const Node &decl); // decl : fun_decl | struct_decl | var_decl
        DeclPtr buildVarDecl(const Node &var_decl);
        DeclPtr buildFunDecl(const Node &fun_decl);
        DeclPtr buildStructDecl(const Node &struct_decl);
        TypePtr buildType(const Node &type);          // type : ptr_type | primitive_type
        TypePtr buildPrimitiveType(const Node &prim); // primitive_type : atomic_type | named_type
        StmtPtr buildStmt(const Node &stmt);          // stmt : expr | comp_stmt
        StmtPtr buildCompound(const Node &comp_stmt); // comp_stmt : { (stmt ;)* }
        StmtPtr buildAssign(const Node &assign_stmt); // comp_stmt : { (stmt ;)* }
        StmtPtr buildReturn(const Node &return_stmt); // comp_stmt : { (stmt ;)* }
        ExprPtr buildExpr(const Node &node);          // expr cascade
        ExprPtr foldTail(ExprPtr lhs, const Node &tail);
    };
}
