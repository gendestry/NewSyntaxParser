#pragma once
#include "Utils/Logging/Logger.h"
#include "FileParser.h"
#include "SyntaxParser/Tokenizer/Token.h"

namespace Parsing::Tokenizer
{
    class Parser
    {
        Parsing::Tokenizer::FileParser file;
        Utils::Logger logger;

        std::vector<Token> v_tokens;
        bool b_parsedTokens = false;

    public:
        Parser(const std::string &token_file);

        bool parseTokens(const std::string &input_file);

        bool parse(const std::string &input_file);

        // Lex directly from an in-memory string (clears the token buffer first),
        // instead of reading a file. Useful for REPLs / interactive input.
        bool parseString(const std::string &input);

        std::vector<Token> &getTokens();
    };
}