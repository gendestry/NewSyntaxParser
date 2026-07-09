// #include "Utils/Logging/Logger.h"

// #include "Tokenizer/Parser.h"

// #include "Syntax/GrammarParser.h"
// #include "Syntax/Engine.h"
// // #include "Syntax/ProgramEvaluator.h"

// #include "Lang/Demo.h"

// #include <exception>
// #include <vector>

// int main()
// {
//     Utils::Logger logger("main");
//     Utils::Logger::setLevel(Utils::Logger::DEBUGGING);

//     // --- 1. Lex the input into tokens ------------------------------------
//     Parsing::Tokenizer::Parser parser("tokens.txt");
//     parser.parse("input.txt");

//     // Drop ignored tokens (WHITESPACE) before parsing the grammar.
//     std::vector<Parsing::Tokenizer::Token> tokens;
//     for (auto &t : parser.getTokens())
//         if (!t.ignore)
//             tokens.push_back(t);

//     logger.println("Tokens:");
//     for (auto &t : tokens)
//         logger.println("  {} '{}'", t.name, t.value);

//     // --- 2. Load the grammar & parse into a tree -------------------------
//     // try
//     // {
//     //     auto grammar = Parsing::Syntax::GrammarParser::parseFile("lang.syn");

//     //     Parsing::Syntax::Engine engine(grammar, tokens);
//     //     auto tree = engine.parse(grammar.startRule);

//     //     if (!tree)
//     //     {
//     //         if (const auto *t = engine.furthestToken())
//     //             logger.error("Parse error near {} '{}' (token #{})",
//     //                          t->name, t->value, engine.furthestPos());
//     //         else
//     //             logger.error("Parse error: unexpected end of input");
//     //         return 1;
//     //     }

//     //     logger.println("\nParse tree:");
//     //     Parsing::Syntax::printTree(*tree);

//     //     // --- 3. Walk the tree with a visitor -----------------------------
//     //     Parsing::Syntax::ProgramEvaluator evaluator;
//     //     double result = evaluator.visit(*tree);
//     //     logger.println("\nResult = {}", result);
//     // }
//     // catch (const std::exception &e)
//     // {
//     //     logger.error("{}", e.what());
//     //     return 1;
//     // }

//     // // --- 4. Typed-AST interpreter demo (the new C-like evaluator) ---------
//     // Lang::runDemo();

//     return 0;
// }

// Standalone runner for the lang_* explanation code. Compiled by hand, NOT part
// of the CMake build. Parses temp/lang_input.txt against lang.syn, builds the
// typed AST, and prints it — no evaluation.
#include "AstBuilder.h"

#include "Tokenizer/Parser.h"
#include "Syntax/GrammarParser.h"
#include "Syntax/Engine.h"

#include <iostream>
#include <vector>

int main()
{
    Parsing::Tokenizer::Parser lexer("tokens.txt");
    lexer.parse("input.txt");

    std::vector<Parsing::Tokenizer::Token> tokens;
    for (auto &t : lexer.getTokens())
        if (!t.ignore)
            tokens.push_back(t);

    auto grammar = Parsing::Syntax::GrammarParser::parseFile("lang.syn");
    Parsing::Syntax::Engine engine(grammar, tokens);
    auto cst = engine.parse(grammar.startRule);
    if (!cst)
    {
        std::cerr << "parse failed\n";
        return 1;
    }

    TempLang::AstBuilder builder;
    TempLang::Program program = builder.buildProgram(*cst);

    std::cout << "\n=== Typed AST for temp/lang_input.txt ===\n";
    for (const auto &decl : program)
        decl->print(2);
    return 0;
}
