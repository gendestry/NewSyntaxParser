#pragma once
// CST -> typed AST lowering. Turns the homogeneous parse tree produced by the
// engine into the Sel:: typed nodes generated from ast.spec.

#include "Ast.gen.h"     // Sel:: Program / Command / ...
#include "Syntax/Node.h" // Parsing::Syntax::Node

namespace Sel {
// entry : command+   -> the whole program.
Program buildProgram(const Parsing::Syntax::Node &entry);
} // namespace Sel
