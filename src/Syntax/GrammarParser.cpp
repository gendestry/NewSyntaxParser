#include "Syntax/GrammarParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <print>
#include <sstream>
#include <stdexcept>

#include "Utils/File/File.h"
#include "Utils/Colors/Theme.h"

namespace Parsing::Syntax
{
    Utils::Logger GrammarParser::logger("GrammarParser");

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
                case ':':
                    // A ':' can never legally appear inside a rule body — the only place
                    // it's valid is between a rule's name and its body, which the caller
                    // has already split off before we get here. Seeing one here almost
                    // always means *this* rule was never terminated with ';', so the next
                    // rule's "name : body" got swallowed into this rule's body instead.
                    throw std::runtime_error(
                        "unexpected ':' in rule body (missing ';' to terminate this rule?)");
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
                        throw std::runtime_error(Utils::Font::format(
                            Utils::Font::group("unterminated literal in '", Theme::name("{}"), "'"),
                            body));
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
                        throw std::runtime_error(Utils::Font::format(
                            Utils::Font::group("unexpected character '", Theme::name("{}"), "' in rule body"),
                            std::string(1, c)));
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
                    throw std::runtime_error("trailing tokens in rule body");
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
                    throw std::runtime_error("empty sequence (misplaced '|' or '()')");
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
                        throw std::runtime_error("missing ')'");
                    advance();
                    return inner;
                }
                throw std::runtime_error("expected a name, literal or '('");
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
        auto linedesc = [&](uint32_t line) {
            return Utils::Font::format(Utils::Font::group(":", Theme::num("{}")), line);
        };

        auto chardesc = [&](const std::string& c) {
            return Utils::Font::format(Utils::Font::group("'", Theme::name(c), "'"));
        };

        // 1. Drop full-line comments, keep everything else, remembering which
        //    original source line each joined line came from.
        std::stringstream joined;
        std::vector<int> origLineOf; // origLineOf[i] == source line number of joined line i
        std::stringstream in(text);
        std::string line;
        int lineNo = 0;
        while (std::getline(in, line))
        {
            lineNo++;
            std::string t = trim(line);
            if (t.empty() || t[0] == '#')
                continue;
            joined << line << '\n';
            origLineOf.push_back(lineNo);
        }

        const std::string all = joined.str();

        // Maps an offset into `all` back to the original source line number.
        auto lineAt = [&](std::size_t offset) -> int
        {
            std::size_t joinedLine = std::count(all.begin(), all.begin() + std::min(offset, all.size()), '\n');
            if (joinedLine >= origLineOf.size())
                joinedLine = origLineOf.empty() ? 0 : origLineOf.size() - 1;
            return origLineOf.empty() ? 0 : origLineOf[joinedLine];
        };

        Grammar grammar;

        // 2. Split into rules on ';'.
        std::size_t start = 0;
        while (true)
        {
            std::size_t semi = all.find(';', start);
            if (semi == std::string::npos)
            {
                if (!trim(all.substr(start)).empty())
                    throw std::runtime_error(Utils::Font::format(
                        Utils::Font::group(linedesc(lineAt(start)), ": rule not terminated with {}"),
                        chardesc(";")));
                break;
            }
            std::size_t chunkStart = start;
            std::string chunk = all.substr(start, semi - start);
            start = semi + 1;

            std::string ruleText = trim(chunk);
            if (ruleText.empty())
                continue;

            std::size_t leading = chunk.find_first_not_of(" \t\r\n");
            const int ruleLine = lineAt(chunkStart + (leading == std::string::npos ? 0 : leading));

            // 3. Split `name : body`.
            std::size_t colon = ruleText.find(':');
            if (colon == std::string::npos)
                throw std::runtime_error(Utils::Font::format(
                    Utils::Font::group(linedesc(ruleLine), ": rule missing {} in '{}'"),
                    chardesc(":"), ruleText));

            std::string name = trim(ruleText.substr(0, colon));
            std::string body = ruleText.substr(colon + 1);
            if (name.empty())
                throw std::runtime_error(Utils::Font::format(
                    Utils::Font::group(linedesc(ruleLine), ": rule with empty name")));

            // 4. Parse the body into a Symbol tree.
            try
            {
                auto meta = lex(body);
                Symbol sym = BodyParser(meta).parse();

                if (grammar.startRule.empty())
                    grammar.startRule = name;
                grammar.rules[name] = Rule{name, std::move(sym)};
            }
            catch (const std::exception &e)
            {
                throw std::runtime_error(Utils::Font::format(
                    Utils::Font::group(
                        linedesc(ruleLine), ": rule '", Theme::name("{}"), "': ", Theme::err(std::string(e.what()))),
                    name));
            }
        }

        // 5. Validate references.
        if (std::string bad = grammar.validate(); !bad.empty())
            throw std::runtime_error(Utils::Font::format(
                Utils::Font::group("reference to undefined rule '", Theme::name("{}"), "'"), bad));

        return grammar;
    }

    std::optional<Grammar> GrammarParser::parseFile(const std::string &path)
    {
        std::ifstream f(path);
        if (!f) {
            logger.error("Cannot open file: '{}'", Theme::name(path));
            return std::nullopt;
            // throw std::runtime_error("grammar: cannot open file '" + path + "'");
        }
        std::stringstream ss;
        ss << f.rdbuf();

        logger.info("Parsing file: '{}'", Theme::name(path));

        try {
            auto p = parseText(ss.str());
            return p;
        }
        catch (std::exception &e) {
            logger.error("File '{}'{}", Theme::name(path), e.what());
        }

        return std::nullopt;

    }
}
