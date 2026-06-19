#include "Parser.h"
#include "Utils/File/File.h"
#include "Utils/Text/Stream.h"
#include "Utils/Text/LineCounter.h"
#include <exception>

namespace Parsing::Tokenizer
{
    std::string Token::toString() const
    {
        // return Utils::String::format("[{} {}:{}] '{}'", name, col, row, value);
        return Utils::String::format("[{}] '{}'", name, value);
    }

    Parser::Parser(const std::string &token_file)
        : logger("Tokenizer")
    {
        b_parsedTokens = parseTokens(token_file);
    }

    bool Parser::parseTokens(const std::string &input_file)
    {
        try
        {
            logger.debug("Parsing tokens from file: '{}'", input_file);
            file = FileParser(input_file);
            file.parse();
        }
        catch (const std::exception &e)
        {
            logger.error("Error parsing tokens: '{}'", e.what());
            return false;
        }
        return true;
    }

    bool Parser::parse(const std::string &input_file)
    {
        if (!b_parsedTokens)
        {
            return false;
        }

        logger.debug("Parsing input from file: '{}'", input_file);

        auto input = Utils::File::read(input_file);
        if (!input.has_value())
        {
            logger.error("{}", input.error());
            return false;
        }

        int pos = 0, prevpos = 0;
        auto sinput = input.value();

        Utils::Text::LineCounter lineCounter;
        lineCounter.count(sinput);

        auto col = lineCounter.getXOffset(pos); // THIS IS WRONG
        auto row = lineCounter.accumulate(pos);

        while (true)
        {
            bool matched = false;
            const auto s = sinput.substr(pos);

            for (auto &token : file.getTokenMaps())
            {
                auto match = token.regex.findInfo(s);
                if (match.has_value())
                {
                    auto v = match.value();
                    if (v.start == 0)
                    {
                        auto matchLen = v.match.length();
                        // auto lb = lineCounter.numLinesInBetween(pos, pos + matchLen);
                        col = lineCounter.getXOffset(pos); // THIS IS WRONG
                        row = lineCounter.accumulate(pos);

                        logger.debug("Match token {}: '{}'", token.tokenName, v.match == "\n" ? "\\n" : v.match);
                        v_tokens.emplace_back(pos, pos + v.match.length(), token.tokenName, v.match, token.ignore, col, row);
                        prevpos = pos;
                        pos += v.match.length();

                        matched = true;

                        if (pos == sinput.length())
                        {
                            logger.println("DELLLLA");
                            b_parsedTokens = true;
                            return true;
                        }
                        break;
                    }
                }
            }

            if (!matched)
            {
                break;
            }
        }

        logger.error("Invalid token on line: {}, column: {}", row, col);
        return false;
    }

    std::vector<Token> &Parser::getTokens()
    {
        return v_tokens;
    }
}