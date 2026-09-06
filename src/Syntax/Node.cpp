#include "Syntax/Node.h"

#include <iostream>
#include <string>
#include "Utils/Colors/Font.h"
#include "Utils/Text/Stream.h"

namespace Parsing::Syntax
{
    namespace
    {
        // Render one line: "<rule>" for interior nodes, "[NAME] 'value'" for leaves.
        std::string label(const Node &node)
        {
            Utils::Text::Stream s;
            if (node.isLeaf())
            {
                const auto &t = *node.token;
                s << Utils::Font::colorBlue << "[" << t.name << "]" << Utils::Font::colorReset
                  << " " << Utils::Font::colorYellow << "'" << t.value << "'" << Utils::Font::colorReset;
            }
            else
            {
                s << Utils::Font::colorMagenta << node.rule << Utils::Font::colorReset;
            }
            return s.end();
        }

        // `prefix` is the indentation drawn for this node's children lines.
        // `isLast` picks the connector glyph for this node itself.
        void print(const Node &node, const std::string &prefix, bool isLast, bool isRoot)
        {
            Utils::Text::Stream line;
            if (!isRoot)
                line << Utils::Font::colorDim << (isLast ? "└── " : "├── ") << Utils::Font::colorReset;

            // Collapse single-child rule chains (expr -> plusexpr -> mulexpr -> atom ...)
            // onto one line so the interesting structure isn't buried in indentation.
            const Node *cur = &node;
            line << label(*cur);
            while (!cur->isLeaf() && cur->kids.size() == 1)
            {
                cur = &cur->kids[0];
                line << Utils::Font::colorDim << " → " << Utils::Font::colorReset << label(*cur);
            }
            std::cout << prefix << line.end() << std::endl;

            if (cur->isLeaf())
                return;

            std::string childPrefix = prefix + (isRoot ? "" : (isLast ? "    " : "│   "));
            for (std::size_t i = 0; i < cur->kids.size(); ++i)
                print(cur->kids[i], childPrefix, i + 1 == cur->kids.size(), false);
        }
    }

    void printTree(const Node &node, int depth)
    {
        print(node, std::string(depth * 4, ' '), true, true);
    }
}
