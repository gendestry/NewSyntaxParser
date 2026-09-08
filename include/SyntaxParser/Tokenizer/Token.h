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

        // A token the parser emitted but that carries no meaning downstream
        // (whitespace, comments) is marked ignore; everything else is enabled.
        [[nodiscard]] bool enabled() const { return !ignore; }

        [[nodiscard]] std::string toString() const override;
    };
}
