# =============================================================================
#  ast.spec — declarative description of the TempLang typed AST.
#  Consumed by gen_ast.py, which emits build/gen/Ast.gen.h (visitor pattern).
#  Do not hand-edit the generated header; edit this file and regenerate.
# =============================================================================
#
#  Field type grammar:
#     string       -> std::string
#     i64          -> long long                 (gets a "= 0" default)
#     <Category>   -> <Category>Ptr             (unique_ptr to the base)
#     <Category>[] -> std::vector<<Category>Ptr>
#     <Record>     -> <Record>                  (plain value)
#     <Record>[]   -> std::vector<<Record>>
#
#  Fields inside a { } block may be separated by ';' or newlines.

namespace TempLang

# Program is the tree root: a flat list of declarations.
program Decl[]

# ---- plain records (emitted as-is, never visited) --------------------------
record Param {
    Type   type
    string name
}

# ---- node categories (each -> base class + visitor interface) --------------
category Type {
    AtomicType  { string prim }
    PointerType { Type pointee }
    NamedType   { string name }
}

category Expr {
    NumberExpr { i64 value }
    NamedExpr  { string value }
    BinaryExpr { string op; Expr lhs; Expr rhs }
    UnaryExpr  { string op; Expr operand }
    CallExpr   { string callee; Expr[] args }
    MemberExpr { Expr base; string member }
}

category Stmt {
    ExprStmt     { Expr expr }
    CompoundStmt { Stmt[] body }
    AssignStmt   { Expr e1; Expr e2 }
    ReturnStmt   { Expr ret }
    VarDeclStmt  { Decl decl }
}

category Decl {
    VarDecl    { Type type; string name; Expr init }
    StructDecl { string name; Decl[] fields }
    FunDecl    { Type returnType; string name; Param[] params; Stmt body }
}
