#include "Syntax/Node.h"

#include <iostream>
#include <string>

namespace Parsing::Syntax
{
    void printTree(const Node &node, int depth)
    {
        std::string indent(depth * 2, ' ');

        if (node.isLeaf())
        {
            const auto &t = *node.token;
            // Show name, value and the source position we kept from the lexer.
            std::cout << indent << t.name << " '" << t.value << "'"
                      << "  @" << t.row << ":" << t.col << "\n";
        }
        else
        {
            std::cout << indent << node.rule << "\n";
            for (const auto &k : node.kids)
                printTree(k, depth + 1);
        }
    }
}
