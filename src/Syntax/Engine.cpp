#include "Syntax/Engine.h"

namespace Parsing::Syntax
{
    Engine::Engine(const Grammar &grammar, std::vector<Tokenizer::Token> tokens)
        : m_grammar(grammar), m_tokens(std::move(tokens))
    {
    }

    const Tokenizer::Token *Engine::furthestToken() const
    {
        if (m_furthest < m_tokens.size())
            return &m_tokens[m_furthest];
        return nullptr;
    }

    std::optional<Node> Engine::parse(const std::string &startRule)
    {
        m_cur = 0;
        m_furthest = 0;

        auto it = m_grammar.rules.find(startRule);
        if (it == m_grammar.rules.end())
            return std::nullopt;

        Node root;
        root.rule = startRule;

        // Must match the start rule AND consume the whole token stream.
        if (match(it->second.body, root) && m_cur == m_tokens.size())
            return root;

        return std::nullopt;
    }

    bool Engine::match(const Symbol &s, Node &out)
    {
        const std::size_t savePos = m_cur;
        const std::size_t saveKids = out.kids.size();

        // Restore to the pre-attempt state (used by every failing branch).
        auto rollback = [&]
        {
            m_cur = savePos;
            out.kids.resize(saveKids);
        };

        switch (s.kind)
        {
        case SymKind::Terminal:
            if (m_cur < m_tokens.size() && m_tokens[m_cur].name == s.text)
            {
                out.kids.push_back(Node{"", m_tokens[m_cur], {}});
                m_cur++;
                m_furthest = std::max(m_furthest, m_cur);
                return true;
            }
            return false;

        case SymKind::Literal:
            if (m_cur < m_tokens.size() && m_tokens[m_cur].value == s.text)
            {
                out.kids.push_back(Node{"", m_tokens[m_cur], {}});
                m_cur++;
                m_furthest = std::max(m_furthest, m_cur);
                return true;
            }
            return false;

        case SymKind::RuleRef:
        {
            auto it = m_grammar.rules.find(s.text);
            if (it == m_grammar.rules.end())
                return false; // validate() should have caught this

            Node child;
            child.rule = s.text;
            if (match(it->second.body, child))
            {
                out.kids.push_back(std::move(child));
                return true;
            }
            rollback();
            return false;
        }

        case SymKind::Seq:
            for (const auto &c : s.children)
            {
                if (!match(c, out))
                {
                    rollback();
                    return false;
                }
            }
            return true;

        case SymKind::Choice:
            for (const auto &c : s.children)
            {
                if (match(c, out))
                    return true;
                rollback();
            }
            return false;

        case SymKind::Star:
            while (true)
            {
                const std::size_t p = m_cur, k = out.kids.size();
                if (!match(s.children[0], out))
                {
                    m_cur = p;
                    out.kids.resize(k);
                    break;
                }
                if (m_cur == p) // matched without consuming -> guard against infinite loop
                    break;
            }
            return true;

        case SymKind::Plus:
        {
            if (!match(s.children[0], out))
            {
                rollback();
                return false;
            }
            while (true)
            {
                const std::size_t p = m_cur, k = out.kids.size();
                if (!match(s.children[0], out))
                {
                    m_cur = p;
                    out.kids.resize(k);
                    break;
                }
                if (m_cur == p)
                    break;
            }
            return true;
        }

        case SymKind::Optional:
        {
            const std::size_t p = m_cur, k = out.kids.size();
            if (!match(s.children[0], out))
            {
                m_cur = p;
                out.kids.resize(k);
            }
            return true;
        }
        }

        return false;
    }
}
