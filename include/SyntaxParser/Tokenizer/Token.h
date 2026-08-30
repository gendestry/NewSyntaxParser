#pragma once
#include <string>
#include "Utils/Traits/Stringify.h"

namespace Parsing::Tokenizer {
    struct Token : public Utils::Traits::Stringify
    {
        unsigned int start, end;
        std::string name;
        std::string value;
        bool ignore = false;
        unsigned int col, row;

        Token() = default;
        Token(unsigned int start, unsigned int end, std::string name, std::string value,
              bool ignore, unsigned int col, unsigned int row)
            : start(start), end(end), name(std::move(name)), value(std::move(value)),
              ignore(ignore), col(col), row(row)
        {
        }

        [[nodiscard]] std::string toString() const override;
    };
}
