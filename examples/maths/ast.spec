# =============================================================================
#  ast.spec — declarative description of the Basic arithmetic AST.
#  Consumed by gen_ast.py, which emits <build>/gen/Ast.gen.h (visitor pattern).
#  Do not hand-edit the generated header; edit this file and regenerate.
# =============================================================================
#
#  Field type grammar:
#     string       -> std::string
#     i64          -> long long                 (gets a "= 0" default)
#     <Category>   -> <Category>Ptr             (unique_ptr to the base)
#     <Category>[] -> std::vector<<Category>Ptr>

namespace Basic

# The tree root is a single expression.
program Expr

# ---- node categories (each -> base class + visitor interface) --------------
category Expr {
    NumberExpr { i64 value }
    BinaryExpr { string op; Expr lhs; Expr rhs }
}
