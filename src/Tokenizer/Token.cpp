//
// Created by bobi on 30. 8. 26.
//
#include "SyntaxParser/Tokenizer/Token.h"
#include "Utils/Colors/Theme.h"

namespace Parsing::Tokenizer
{
std::string Token::toString() const
{
    return format(group(Theme::name("{}"),"[", Theme::num("{}"), "]({},{})"), name, value, col, col + value.size());
}
} // namespace Parsing::Tokenizer