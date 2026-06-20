#include "Syntax/GrammarParser.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace Parsing::Syntax
{
    namespace
    {
        // ---- meta-lexer: turn a rule body into a stream of meta-tokens ------
        enum class MTok
        {
            Name,    // expr, NUM, PLUS ...
            Literal, // '+'
            LParen,
            RParen,
            Pipe,
            Star,
            Plus,
            Question,
            End,
        };

        struct Meta
        {
            MTok kind;
            std::string text; // for Name/Literal
        };

        bool isNameStart(char c) { return std::isalpha((unsigned char)c) || c == '_'; }
        bool isNameChar(char c) { return std::isalnum((unsigned char)c) || c == '_'; }

        std::vector<Meta> lex(const std::string &body)
        {
            std::vector<Meta> out;
            std::size_t i = 0;
            while (i < body.size())
            {
                char c = body[i];
                if (std::isspace((unsigned char)c))
                {
                    i++;
                    continue;
                }
                switch (c)
                {
                case '(': out.push_back({MTok::LParen, ""}); i++; break;
                case ')': out.push_back({MTok::RParen, ""}); i++; break;
                case '|': out.push_back({MTok::Pipe, ""}); i++; break;
                case '*': out.push_back({MTok::Star, ""}); i++; break;
                case '+': out.push_back({MTok::Plus, ""}); i++; break;
                case '?': out.push_back({MTok::Question, ""}); i++; break;
                case '\'':
                {
                    std::size_t j = i + 1;
                    std::string val;
                    while (j < body.size() && body[j] != '\'')
                    {
                        if (body[j] == '\\' && j + 1 < body.size()) // allow \' and \\
                            j++;
                        val += body[j++];
                    }
                    if (j >= body.size())
                        throw std::runtime_error("grammar: unterminated literal in '" + body + "'");
                    out.push_back({MTok::Literal, val});
                    i = j + 1;
                    break;
                }
                default:
                    if (isNameStart(c))
                    {
                        std::size_t j = i;
                        while (j < body.size() && isNameChar(body[j]))
                            j++;
                        out.push_back({MTok::Name, body.substr(i, j - i)});
                        i = j;
                    }
                    else
                    {
                        throw std::runtime_error(
                            std::string("grammar: unexpected character '") + c + "' in rule body");
                    }
                }
            }
            out.push_back({MTok::End, ""});
            return out;
        }

        // ---- meta-parser: recursive descent over the meta-tokens ------------
        // body    := choice
        // choice  := seq ('|' seq)*
        // seq     := postfix+
        // postfix := primary ('*' | '+' | '?')?
        // primary := Name | Literal | '(' choice ')'
        class BodyParser
        {
            const std::vector<Meta> &m_t;
            std::size_t m_i = 0;

        public:
            explicit BodyParser(const std::vector<Meta> &t) : m_t(t) {}

            Symbol parse()
            {
                Symbol s = parseChoice();
                if (peek().kind != MTok::End)
                    throw std::runtime_error("grammar: trailing tokens in rule body");
                return s;
            }

        private:
            const Meta &peek() const { return m_t[m_i]; }
            const Meta &advance() { return m_t[m_i++]; }
            bool check(MTok k) const { return peek().kind == k; }

            bool startsPrimary() const
            {
                return check(MTok::Name) || check(MTok::Literal) || check(MTok::LParen);
            }

            Symbol parseChoice()
            {
                std::vector<Symbol> alts;
                alts.push_back(parseSeq());
                while (check(MTok::Pipe))
                {
                    advance();
                    alts.push_back(parseSeq());
                }
                if (alts.size() == 1)
                    return alts.front();
                return choice(std::move(alts));
            }

            Symbol parseSeq()
            {
                std::vector<Symbol> items;
                while (startsPrimary())
                    items.push_back(parsePostfix());
                if (items.empty())
                    throw std::runtime_error("grammar: empty sequence (misplaced '|' or '()')");
                if (items.size() == 1)
                    return items.front();
                return seq(std::move(items));
            }

            Symbol parsePostfix()
            {
                Symbol p = parsePrimary();
                if (check(MTok::Star)) { advance(); return star(std::move(p)); }
                if (check(MTok::Plus)) { advance(); return plus(std::move(p)); }
                if (check(MTok::Question)) { advance(); return optional(std::move(p)); }
                return p;
            }

            Symbol parsePrimary()
            {
                if (check(MTok::Name))
                {
                    const std::string name = advance().text;
                    // ANTLR convention: UPPERCASE first letter => terminal/token.
                    if (std::isupper((unsigned char)name[0]))
                        return terminal(name);
                    return ruleRef(name);
                }
                if (check(MTok::Literal))
                    return literal(advance().text);
                if (check(MTok::LParen))
                {
                    advance();
                    Symbol inner = parseChoice();
                    if (!check(MTok::RParen))
                        throw std::runtime_error("grammar: missing ')'");
                    advance();
                    return inner;
                }
                throw std::runtime_error("grammar: expected a name, literal or '('");
            }
        };

        std::string trim(const std::string &s)
        {
            std::size_t a = s.find_first_not_of(" \t\r\n");
            if (a == std::string::npos)
                return "";
            std::size_t b = s.find_last_not_of(" \t\r\n");
            return s.substr(a, b - a + 1);
        }
    } // namespace

    Grammar GrammarParser::parseText(const std::string &text)
    {
        // 1. Drop full-line comments, keep everything else.
        std::stringstream joined;
        std::stringstream in(text);
        std::string line;
        while (std::getline(in, line))
        {
            std::string t = trim(line);
            if (t.empty() || t[0] == '#')
                continue;
            joined << line << '\n';
        }

        Grammar grammar;
        const std::string all = joined.str();

        // 2. Split into rules on ';'.
        std::size_t start = 0;
        while (true)
        {
            std::size_t semi = all.find(';', start);
            if (semi == std::string::npos)
            {
                if (!trim(all.substr(start)).empty())
                    throw std::runtime_error("grammar: rule not terminated with ';'");
                break;
            }
            std::string chunk = all.substr(start, semi - start);
            start = semi + 1;

            std::string ruleText = trim(chunk);
            if (ruleText.empty())
                continue;

            // 3. Split `name : body`.
            std::size_t colon = ruleText.find(':');
            if (colon == std::string::npos)
                throw std::runtime_error("grammar: rule missing ':' in '" + ruleText + "'");

            std::string name = trim(ruleText.substr(0, colon));
            std::string body = ruleText.substr(colon + 1);
            if (name.empty())
                throw std::runtime_error("grammar: rule with empty name");

            // 4. Parse the body into a Symbol tree.
            auto meta = lex(body);
            Symbol sym = BodyParser(meta).parse();

            if (grammar.startRule.empty())
                grammar.startRule = name;
            grammar.rules[name] = Rule{name, std::move(sym)};
        }

        // 5. Validate references.
        if (std::string bad = grammar.validate(); !bad.empty())
            throw std::runtime_error("grammar: reference to undefined rule '" + bad + "'");

        return grammar;
    }

    Grammar GrammarParser::parseFile(const std::string &path)
    {
        std::ifstream f(path);
        if (!f)
            throw std::runtime_error("grammar: cannot open file '" + path + "'");
        std::stringstream ss;
        ss << f.rdbuf();
        return parseText(ss.str());
    }
}
