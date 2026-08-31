//
// Created by bobi on 30. 8. 26.
//
#include "SyntaxParser/Tokenizer/Token.h"
#include "Utils/Text/String.h"


namespace Parsing::Tokenizer
{
std::string Token::toString() const
{
    return name + "(" + value + ")";
}
} // namespace Parsing::Tokenizer