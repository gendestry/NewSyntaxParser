#include "Syntax/Node.h"

#include <iostream>
#include <string>
#include "Utils/Colors/Font.h"
#include "Utils/Text/Stream.h"

namespace Parsing::Syntax
{
    void printTree(const Node &node, int depth)
    {
        std::string indent(depth, ' ');

        if (node.isLeaf())
        {
            Utils::Text::Stream s;
            const auto &t = *node.token;
            s << indent;
            s << Utils::Font::colorBlue << "[" << t.name << "] " << Utils::Font::colorReset;
            s << Utils::Font::colorYellow << "'" << t.value << "'" << Utils::Font::colorReset;
            // Show name, value and the source position we kept from the lexer.
            // std::cout << indent << t.name << " '" << t.value << "'"
            //           << "  @" << t.row << ":" << t.col << "\n";
            std::cout << s.end() << std::endl;
        }
        else
        {
            Utils::Text::Stream s;
            s << indent;
            s << Utils::Font::colorMagenta << node.rule << Utils::Font::colorReset;
            std::cout << s.end() << std::endl;
            for (const auto &k : node.kids)
                printTree(k, depth + 1);
        }
    }
}
