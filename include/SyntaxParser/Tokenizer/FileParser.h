#pragma once
#include <string>
#include "Utils/Regex/Matcher.h"
#include "Utils/Logging/Logger.h"

namespace Parsing::Tokenizer
{

    struct TokenMaper
    {
        std::string tokenName;
        Utils::Regex::Matcher regex;
        bool ignore = false;
    };

    using TokenMap = std::vector<TokenMaper>;

    class FileParser
    {
        std::string m_FilePath;
        std::string m_FileContent;

        Utils::Logger logger;

        TokenMap tokenMaps;

    public:
        FileParser() : logger("FileParser") {};
        FileParser(const std::string &file_path);

        // bool parse();
        bool parse();
        void print();

        inline TokenMap &getTokenMaps()
        {
            return tokenMaps;
        }
    };
};