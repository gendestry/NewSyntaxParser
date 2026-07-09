#pragma once
// The typed AST + interpreter now live under Tree/:
//   Tree/Builder.{h,cpp}     CST -> Sel:: typed AST
//   Tree/Context.{h,cpp}     interpreter state
//   Tree/Resolver.{h,cpp}    Selector -> fixture ids
//   Tree/Interpreter.{h,cpp} Command visitor + guards
