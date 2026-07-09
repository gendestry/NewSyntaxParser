#include "FileParser.h"
#include <fstream>
#include <iostream>

#include "Utils/Colors/Font.h"

using namespace Utils;
using Utils::Regex::Matcher;

namespace Parsing::Tokenizer
{
    FileParser::FileParser(const std::string &file_path) : m_FilePath(file_path)
    {
    }

    bool FileParser::parse()
    {
        std::ifstream file(m_FilePath);

        if (!file.is_open())
        {
            throw std::runtime_error("Error: file '" + m_FilePath + "' not found");
            return false;
        }

        std::string line;

        while (std::getline(file, line))
        {
            bool ignore = false;

            if (line[0] == '#' || line.empty())
                continue;

            if (line[0] == '!')
            {
                ignore = true;
                line = line.substr(1);
            }

            size_t namePos = line.find_first_of("= ");
            std::string name = line.substr(0, namePos);

            size_t patternPos = line.find_first_of("= ", namePos + 1);
            while (line[patternPos] == ' ' || line[patternPos] == '=')
                patternPos++;
            std::string pattern = line.substr(patternPos);

            tokenMaps.push_back({name, Matcher(pattern), ignore});
        }
        return false;
        // Parse the file content
    }

    void FileParser::print()
    {
        std::cout << Font::colorGreen << " ==== INPUT TOKENS ==== " << Font::colorReset << std::endl;
        for (auto &token : tokenMaps)
        {
            std::cout << Font::colorYellow << "'" << token.tokenName << "'" << Font::colorReset << ": "
                      << Font::colorBlue << token.regex.getPattern() << Font::colorReset;

            if (token.ignore)
                std::cout << Font::colorRed << " [IGNORED]" << Font::colorReset;

            std::cout << std::endl;
        }
    }
};