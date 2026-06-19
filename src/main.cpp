#include "Utils/Logging/Logger.h"
#include "Utils/File/File.h"
#include "Utils/Text/String.h"

#include "Tokenizer/Parser.h"

int main()
{
    Utils::Logger logger("main");
    Utils::Logger::setLevel(Utils::Logger::DEBUGGING);
    Parsing::Tokenizer::Parser parser("tokens.txt");

    parser.parse("input.txt");

    for (auto &token : parser.getTokens())
    {
        if (token.ignore)
            continue;
        logger.println("{}", token.toString());
        // logger.println("[{}] S:{}, E:{}, val:'{}', Ign:{}", token.name, token.start, token.end, token.value, token.ignore ? "true" : "false");
    }

    return 0;
}