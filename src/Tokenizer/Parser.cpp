#include "SyntaxParser/Tokenizer/Parser.h"
#include <exception>
#include <ranges>
#include <sstream>
#include "Utils/File/File.h"
#include "Utils/Text/LineCounter.h"

namespace Parsing::Tokenizer
{
    Parser::Parser(const std::string &token_file)
        : logger("Tokenizer")
    {
        b_parsedTokens = parseTokens(token_file);
    }

    bool Parser::parseTokens(const std::string &input_file)
    {

        logger.info("Parsing tokens from file: '{}'", Theme::name(input_file));
        file = FileParser(input_file);
        return file.parse();
    }

    bool Parser::parse(const std::string &input_file)
    {
        if (!b_parsedTokens)
        {
            return false;
        }

        logger.info("Parsing input from file: '{}'", Theme::name(input_file));

        auto input = Utils::File::read(input_file);
        if (!input.has_value())
        {
            logger.error("{}", input.error());
            return false;
        }

        return parseString(input.value());
    }

    bool Parser::parseString(const std::string &input)
    {
        if (!b_parsedTokens)
        {
            return false;
        }

        // Start fresh so repeated calls (e.g. a REPL) don't accumulate tokens.
        v_tokens.clear();

        int pos = 0, prevpos = 0;
        auto sinput = input;

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

                        v_tokens.emplace_back(pos, pos + v.match.length(), token.tokenName, v.match, token.ignore, col, row);
                        logger.debug("Match token {}", v_tokens.back().toString());
                        // logger.debug("Match token {}: '{}'", token.tokenName, v.match == "\n" ? "\\n" : v.match);
                        prevpos = pos;
                        pos += v.match.length();

                        matched = true;

                        if (pos == sinput.length())
                        {
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

    std::string Parser::toString() const
    {
        std::stringstream ss;

        const Token *prev = nullptr;
        uint16_t r = 0;
        for (const auto &token : v_tokens | std::views::filter([](const Token &t) { return t.enabled(); }))
        {
            if (prev)
            {
                // if (token.row != prev->row)
                ss << (token.row != prev->row ? std::format("\n{:3}: ", token.row) : " ");
            }
            else {
                ss << std::format("{:3}: ", token.row);
            }


            ss << token.toString();
            prev = &token;
        }

        return ss.str();
    }
}