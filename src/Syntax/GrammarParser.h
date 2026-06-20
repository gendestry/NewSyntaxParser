#pragma once
#include <string>

#include "Syntax/Grammar.h"

namespace Parsing::Syntax
{
    // Reads an ANTLR-flavoured EBNF grammar file into a Grammar.
    //
    // Grammar syntax (one rule per `... ;`, '#' starts a full-line comment):
    //
    //     expr   : term ((PLUS | MINUS) term)* ;
    //     term   : factor ((MUL | DIV) factor)* ;
    //     factor : NUM | LPAREN expr RPAREN ;
    //
    //   UPPERCASE name  -> Terminal  (matches a token by its NAME)
    //   lowercase name  -> RuleRef   (matches another rule)
    //   'quoted'        -> Literal   (matches a token by its VALUE)
    //   ( )  grouping    |  alternation    *  zero-or-more
    //   +  one-or-more   ?  optional
    //
    // Throws std::runtime_error with a descriptive message on a malformed file.
    class GrammarParser
    {
    public:
        static Grammar parseFile(const std::string &path);
        static Grammar parseText(const std::string &text);
    };
}
