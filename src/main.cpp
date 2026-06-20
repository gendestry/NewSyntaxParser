#include "Utils/Logging/Logger.h"

#include "Tokenizer/Parser.h"

#include "Syntax/GrammarParser.h"
#include "Syntax/Engine.h"
#include "Syntax/Evaluator.h"

#include <exception>
#include <vector>

struct Int10
{
    int16_t val;
    ~val 0000 0011 1111 1111
};

int main()
{
    Utils::Logger logger("main");
    Utils::Logger::setLevel(Utils::Logger::DEBUGGING);

    // --- 1. Lex the input into tokens ------------------------------------
    Parsing::Tokenizer::Parser parser("tokens.txt");
    parser.parse("input.txt");

    // Drop ignored tokens (WHITESPACE) before parsing the grammar.
    std::vector<Parsing::Tokenizer::Token> tokens;
    for (auto &t : parser.getTokens())
        if (!t.ignore)
            tokens.push_back(t);

    logger.println("Tokens:");
    for (auto &t : tokens)
        logger.println("  {} '{}'", t.name, t.value);

    // --- 2. Load the grammar & parse into a tree -------------------------
    try
    {
        auto grammar = Parsing::Syntax::GrammarParser::parseFile("lang.syn");

        Parsing::Syntax::Engine engine(grammar, tokens);
        auto tree = engine.parse(grammar.startRule);

        if (!tree)
        {
            if (const auto *t = engine.furthestToken())
                logger.error("Parse error near {} '{}' (token #{})",
                             t->name, t->value, engine.furthestPos());
            else
                logger.error("Parse error: unexpected end of input");
            return 1;
        }

        logger.println("\nParse tree:");
        Parsing::Syntax::printTree(*tree);

        // --- 3. Walk the tree with a visitor -----------------------------
        Parsing::Syntax::Evaluator evaluator;
        double result = evaluator.visit(*tree);
        logger.println("\nResult = {}", result);
    }
    catch (const std::exception &e)
    {
        logger.error("{}", e.what());
        return 1;
    }

    return 0;
}
