# NewSyntaxParser (`synparser`)

A small, **data-driven parsing toolkit** for C++23. You describe a language in
three plain-text files and get a lexer, a parser, and a typed AST:

| File         | Describes                        | Consumed by                    |
|--------------|----------------------------------|--------------------------------|
| `tokens.txt` | how text becomes tokens          | `Parsing::Tokenizer::Parser`   |
| `lang.syn`   | the grammar (ANTLR-flavoured EBNF) | `Parsing::Syntax::GrammarParser` + `Engine` |
| `ast.spec`   | the shape of your typed AST      | `generator/gen_ast.py` → `Ast.gen.h` |

Nothing about *your* language is baked into the library. The token rules and the
grammar are **interpreted at runtime**, so you can edit `tokens.txt` / `lang.syn`
and re-run the same binary without recompiling. Only the AST spec involves code
generation, and that is a single Python script wired into CMake.

---

## Table of contents

1. [The pipeline](#the-pipeline)
2. [Repository layout](#repository-layout)
3. [Building and running](#building-and-running)
4. [Quick start — a language in 60 lines](#quick-start--a-language-in-60-lines)
5. [`tokens.txt` — the lexer spec](#tokenstxt--the-lexer-spec)
6. [`lang.syn` — the grammar spec](#langsyn--the-grammar-spec)
7. [`ast.spec` — the AST spec](#astspec--the-ast-spec)
8. [C++ API reference](#c-api-reference)
9. [The parse tree: how nodes are shaped](#the-parse-tree-how-nodes-are-shaped)
10. [Lowering: CST → typed AST](#lowering-cst--typed-ast)
11. [Walking the AST: visitors and interpreters](#walking-the-ast-visitors-and-interpreters)
12. [The examples](#the-examples)
13. [Using synparser in your own project](#using-synparser-in-your-own-project)
14. [Editor support](#editor-support)
15. [Limitations and gotchas](#limitations-and-gotchas)

---

## The pipeline

```
 input.txt ──┐
             │   ┌──────────────────┐
 tokens.txt ─┴──▶│ Tokenizer::Parser│──▶ vector<Token>      "flat token stream"
                 └──────────────────┘         │
                                              │  (you filter out ignored tokens)
                                              ▼
 lang.syn ──▶ GrammarParser::parseFile ──▶ Grammar
                                              │
                                              ▼
                                     ┌──────────────────┐
                                     │  Syntax::Engine  │──▶ Syntax::Node  "the CST"
                                     └──────────────────┘         │
                                                                  │  (your builder code)
 ast.spec ──▶ gen_ast.py ──▶ Ast.gen.h ───────────────────────────┤
              (typed nodes + visitor interfaces)                  ▼
                                                            typed AST (Program)
                                                                  │
                                                                  ▼
                                                        your Visitor / Interpreter
```

Five stages, each independently usable:

1. **Lex** — `Tokenizer::Parser` turns characters into `Token`s using the regex
   rules from `tokens.txt`.
2. **Load grammar** — `GrammarParser` reads `lang.syn` into a `Grammar` (a map of
   rule name → `Symbol` tree).
3. **Parse** — `Engine` is a backtracking recursive-descent (PEG) matcher that
   runs the `Grammar` over the token stream and produces a **homogeneous** parse
   tree of `Syntax::Node` (a CST: every node looks the same, dispatch is on the
   rule name string).
4. **Lower** — you write a small builder that walks the CST and constructs typed
   nodes. The typed node classes are generated from `ast.spec`.
5. **Walk** — the generated header also emits a `Visitor` interface per node
   category, so an interpreter / printer / type-checker is a `struct X : FooVisitor`.

Stages 4–5 are optional. For a calculator you can evaluate the CST directly with
`Syntax::Visitor<double>` (see `src/Syntax/Evaluator.h`).

---

## Repository layout

```
CMakeLists.txt              library target + the add_synparser_example() helper
generator/gen_ast.py        ast.spec -> Ast.gen.h code generator
lib/Utils/                  git submodule: logging, regex engine, file/text helpers
src/
  Tokenizer/
    FileParser.{h,cpp}      reads tokens.txt into a TokenMap
    Parser.{h,cpp}          the lexer: TokenMap + input -> vector<Token>
    Token.h                 (empty; Token lives in Parser.h)
  Syntax/
    Grammar.h/.cpp          Symbol / Rule / Grammar data model + validate()
    GrammarParser.{h,cpp}   lang.syn -> Grammar
    Engine.{h,cpp}          the PEG matcher: Grammar + tokens -> Node
    Node.{h,cpp}            the homogeneous CST node + printTree()
    Visitor.h               generic Visitor<R> over the CST
    Evaluator.{h,cpp}       worked example: arithmetic evaluator over the CST
examples/
  basic/                    + - * / with parens, CST -> generated AST -> evaluate
  complex/                  C-like language: structs, functions, pointers, members
  test/                     interactive lighting-console REPL with persistent state
editors/vscode/             TextMate highlighting for .syn / ast.spec / tokens.txt
```

---

## Building and running

**Requirements:** a C++23 compiler (tested with GCC 14), CMake ≥ 3.10, Python 3
(for the AST generator), and the `Utils` submodule.

```sh
git submodule update --init --recursive     # pulls lib/Utils

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This produces:

* `build/libsynparser.a` — the reusable library (links `utils`).
* `build/examples/basic/basic`
* `build/examples/complex/complex`
* `build/examples/test/test_example`

Each example binary gets its `tokens.txt`, `lang.syn` and `input.txt` copied next
to it at build time, so **run them from their own build directory**:

```sh
cd build/examples/basic && ./basic
# result = 3

cd build/examples/complex && ./complex
# === Typed AST for temp/lang_input.txt ===
#   StructDecl Data {
#     VarDecl int a (uninitialised)
#     ...

cd build/examples/test && ./test_example
# fixture console — type commands, 'state' to inspect, 'quit' to exit
# > 1 thru 10
# select {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
```

`examples/test` finds its data files via `/proc/self/exe`, so it also works from
anywhere; the other two use plain relative paths.

To regenerate an AST header by hand (CMake normally does this for you):

```sh
python3 generator/gen_ast.py examples/test/ast.spec examples/test/Ast.gen.h
```

---

## Quick start — a language in 60 lines

Say you want to evaluate `1 + 2 * (4 - 3)`.

**1. `tokens.txt`** — one token per line, `!` means "lex it but let the consumer
skip it":

```
NUM = ([1-9][0-9]* | '0'*)
!WHITESPACE = (' ' | \n)+

PLUS = '+'
MINUS = '-'
MUL = '*'
DIV = '/'
LPAREN = '('
RPAREN = ')'
```

**2. `lang.syn`** — the first rule declared is the start rule:

```
entry      : expr ;

expr       : plusexpr n_plusexpr? ;
n_plusexpr : (PLUS | MINUS) plusexpr n_plusexpr? ;

plusexpr   : mulexpr n_mulexpr? ;
n_mulexpr  : (MUL | DIV) mulexpr n_mulexpr? ;

mulexpr    : atom ;
atom       : NUM | group ;
group      : LPAREN expr RPAREN ;
```

**3. `ast.spec`** — what the typed tree should look like:

```
namespace Basic

program Expr

category Expr {
    NumberExpr { i64 value }
    BinaryExpr { string op; Expr lhs; Expr rhs }
}
```

**4. `main.cpp`** — wire the four stages together:

```cpp
#include "Tokenizer/Parser.h"
#include "Syntax/GrammarParser.h"
#include "Syntax/Engine.h"
#include "Ast.gen.h"          // generated from ast.spec

int main()
{
    // 1. Lex.
    Parsing::Tokenizer::Parser lexer("tokens.txt");
    lexer.parse("input.txt");

    std::vector<Parsing::Tokenizer::Token> tokens;
    for (auto &t : lexer.getTokens())
        if (!t.ignore)                       // drop WHITESPACE
            tokens.push_back(t);

    // 2. Load the grammar and 3. parse.
    auto grammar = Parsing::Syntax::GrammarParser::parseFile("lang.syn");
    Parsing::Syntax::Engine engine(grammar, tokens);
    auto cst = engine.parse(grammar.startRule);
    if (!cst)
    {
        if (const auto *t = engine.furthestToken())
            std::cerr << "parse error near " << t->name << " '" << t->value << "'\n";
        else
            std::cerr << "parse error: unexpected end of input\n";
        return 1;
    }

    // 4. Lower to the typed AST, 5. evaluate.
    Basic::ExprPtr ast = buildExpr(cst->kids[0]);   // your builder, see below
    Evaluator evaluator;                            // your visitor, see below
    std::cout << "result = " << evaluator.eval(*ast) << "\n";
}
```

**5. `CMakeLists.txt`** in that directory is a single line:

```cmake
add_synparser_example(basic)
```

The complete, compiling version of this is `examples/basic/`.

---

## `tokens.txt` — the lexer spec

### Line format

```
[!]NAME = <regex>
```

* `NAME` — the token name. Grammar rules refer to it in **UPPERCASE** (the
  grammar parser decides "terminal vs. rule reference" by the first letter's
  case, so token names must start with an uppercase letter).
* `!` prefix — sets `Token::ignore = true`. **The lexer still emits the token**;
  it is the caller's job to filter it out before handing the stream to the
  engine. That is the `if (!t.ignore)` loop in every example.
* The separator is `=` (surrounding spaces are skipped). `NAME <regex>` with just
  a space also parses.
* `#` at the start of a line is a comment. Blank lines are skipped.
  **Inline comments are not supported.**

### Regex dialect

The pattern is compiled by `Utils::Regex::Matcher` — a small hand-written engine,
**not** `std::regex` or PCRE. It supports:

| Form              | Meaning                                             |
|-------------------|-----------------------------------------------------|
| `'text'`          | match the literal text                              |
| `[a-z]`           | character range (the second bound must be ≥ the first) |
| `(a \| b)`        | alternation, grouping                               |

| Escape | Matches                  |
|--------|--------------------------|
| `\c`   | any lowercase letter     |
| `\C`   | any uppercase letter     |
| `\T`   | any character            |
| `\d`   | any digit                |
| `\n`   | newline                  |

| Operator | Meaning              |
|----------|----------------------|
| `*`      | 0 or more            |
| `+`      | 1 or more            |
| `?`      | 0 or 1               |
| `{x,y}`  | between x and y times |

Examples from the shipped token files:

```
NUM        = ([1-9][0-9]* | '0') ('.' [0-9]+)?    # 0, 42, 2.5
IDENTIFIER = \T+                                   # greedy catch-all, listed LAST
!WHITESPACE = (' ' | \n)+
```

### Matching rules — order matters

The lexer walks the token list **in file order** and takes the **first** pattern
that matches at the current position. There is no longest-match / maximal-munch
rule. Consequences:

* **Keywords must come before the catch-all identifier rule.** `STRUCT = 'struct'`
  has to appear above `IDENTIFIER = \T+`, or every keyword lexes as an identifier.
* A more specific token must precede a more general one that shares a prefix.
* A pattern that matches the empty string is a hazard — prefer `'0'` over `'0'*`.

### Tabs

The whitespace rule in the examples is `(' ' | \n)+` — it does **not** cover tabs,
and a tab in the input is a lex error. There is no `\t` escape; write a literal
tab inside quotes instead:

```
!WHITESPACE = (' ' | '	' | \n)+
```

---

## `lang.syn` — the grammar spec

ANTLR-flavoured EBNF. One rule per `... ;`. The **first rule in the file is the
start rule** (`Grammar::startRule`).

```
rulename : <body> ;
```

| In a rule body      | Means                                                   |
|---------------------|---------------------------------------------------------|
| `UPPERCASE`         | **Terminal** — matches a token by its **name**           |
| `lowercase`         | **RuleRef** — matches another rule                       |
| `'quoted'`          | **Literal** — matches a token by its **value**           |
| `a b c`             | sequence                                                 |
| `a \| b`            | ordered choice                                           |
| `( … )`             | grouping                                                 |
| `a*` `a+` `a?`      | zero-or-more, one-or-more, optional                      |
| `# …` on its own line | comment                                                |

Precedence in the meta-grammar: `choice → seq → postfix → primary`, i.e.
`a b | c d` is `(a b) | (c d)`, and `a b*` is `a (b*)`.

`GrammarParser` throws `std::runtime_error` with a message on a malformed file,
and `Grammar::validate()` (called automatically) rejects references to undeclared
rules.

### Semantics: this is a PEG, not an LL/LR grammar

The engine is an ordered-choice backtracking matcher. Three consequences you must
design around:

**1. Choice is ordered and does not re-try after a later failure.** Once an
alternative matches, the parser commits to it at that level; if the enclosing
sequence fails later, the *whole* enclosing rule fails rather than re-entering the
choice with the next alternative. Verified:

```
entry : (NUM | NUM PLUS NUM) ;      # input "1 + 2"  ->  parse error near PLUS '+'
entry : (NUM PLUS NUM | NUM) ;      # input "1 + 2"  ->  parses
```

Rule of thumb: **put the longer alternative first.**

**2. Left recursion is fatal.** `expr : expr PLUS NUM | NUM ;` recurses forever
and segfaults on stack overflow. Express repetition with `*`/`+`, or with the
"head + optional right-recursive tail" idiom the examples use:

```
expr       : plusexpr n_plusexpr? ;
n_plusexpr : (PLUS | MINUS) plusexpr n_plusexpr? ;
```

This leans the tree to the **right**, so your builder folds it **left** to keep
`-` and `/` left-associative (see `foldTail` in `examples/basic/main.cpp`).

**3. `*` and `+` are greedy without backtracking on the count.** They consume as
much as they can and never give a repetition back to help the following symbols
match. (The engine does guard against zero-width matches looping forever.)

**4. The whole token stream must be consumed.** `Engine::parse()` succeeds only if
the start rule matches *and* `m_cur == tokens.size()`. Leftover tokens = parse
failure.

### Encoding precedence

Precedence comes from the rule cascade, tightest-binding last:

```
expr  ->  plusexpr  ->  mulexpr  ->  atom
 + -        * /                      NUM, ( … )
```

---

## `ast.spec` — the AST spec

Read by `generator/gen_ast.py`, which emits a self-contained header with the
typed node structs and one visitor interface per category. The generator knows
nothing about your grammar — it just renders whatever the spec declares.

### Directives

```
namespace <Name>              # required — the C++ namespace to emit into
program   <Type>              # optional — emits `using Program = <Type>;`
record    <Name> { <field>* } # plain struct, no base class, never visited
category  <Name> {            # -> base class + <Name>Visitor + <Name>Ptr alias
    <NodeName> { <field>* }   # -> struct <NodeName> : <Name> with accept()
    ...
}
```

`#` starts a comment (full-line **and** inline — the spec has no string literals).
Fields inside `{ }` are separated by `;` or newlines.

### Field types

```
<Type> <name>
```

| Spec type    | Emitted C++                          | Default |
|--------------|--------------------------------------|---------|
| `string`     | `std::string`                        | —       |
| `bool`       | `bool`                               | `false` |
| `i32` `i64`  | `int`, `long long`                   | `0`     |
| `u32` `u64`  | `unsigned`, `unsigned long long`     | `0`     |
| `f32` `f64`  | `float`, `double`                    | `0`     |
| `<Category>` | `<Category>Ptr` (= `unique_ptr<Category>`) | — |
| `<Record>`   | `<Record>` by value                  | —       |
| anything else| emitted verbatim, by value           | —       |
| `T[]`        | `std::vector<T>`                     | —       |

The builtin table lives at the top of `gen_ast.py` (`BUILTINS`) and is trivial to
extend.

### Worked example

`examples/test/ast.spec`:

```
namespace Sel

program Command[]

category Selector {
    Fixture      { i64 id }
    FixtureRange { i64 from; i64 to }
    Group        { i64 id }
    Preset       { i64 bank; i64 number }
}

record AtValue {
    bool     isPreset
    f64      level
    Selector preset
}

category Command {
    SelectCmd { Item[] items; bool hasAt; AtValue at }
    StoreCmd  { Selector target }
    DeleteCmd { Selector target }
    ClearCmd  { }
    AtCmd     { AtValue at }
}

record Item {
    string   op
    Selector sel
}
```

generates (abridged, see `examples/test/Ast.gen.h` for the full file):

```cpp
namespace Sel
{
    struct Fixture; struct FixtureRange; /* ... forward declarations ... */

    struct SelectorVisitor
    {
        virtual ~SelectorVisitor() = default;
        virtual void visit(Fixture &) = 0;
        virtual void visit(FixtureRange &) = 0;
        virtual void visit(Group &) = 0;
        virtual void visit(Preset &) = 0;
    };

    struct Selector
    {
        virtual ~Selector() = default;
        virtual void accept(SelectorVisitor &v) = 0;
    };
    using SelectorPtr = std::unique_ptr<Selector>;

    struct AtValue                       // record: plain struct, no accept()
    {
        bool isPreset = false;
        double level = 0;
        SelectorPtr preset;
    };

    struct FixtureRange : Selector
    {
        long long from = 0;
        long long to = 0;
        void accept(SelectorVisitor &v) override { v.visit(*this); }
    };

    using Program = std::vector<CommandPtr>;
}
```

Note the emitted structs are **aggregates with public fields and no
constructors** — you build them with `make_unique<T>()` and assign the fields.

`record` vs `category`: a record is data you always know the type of (a parameter,
a key/value pair); a category is a closed set of alternatives you want to
double-dispatch over.

---

## C++ API reference

Everything lives under `namespace Parsing`. Include paths are rooted at `src/`,
which `target_link_libraries(... synparser)` adds for you.

### `Parsing::Tokenizer::Token` — `Tokenizer/Parser.h`

```cpp
struct Token
{
    unsigned int start, end;   // byte offsets into the input
    std::string name;          // the token name from tokens.txt
    std::string value;         // the matched text
    bool ignore = false;       // true for '!'-prefixed token rules
    unsigned int col, row;     // source position (see gotchas — currently unreliable)

    std::string toString() const;   // "[NAME] 'value'"
};
```

### `Parsing::Tokenizer::Parser` — the lexer

```cpp
class Parser
{
public:
    explicit Parser(const std::string &token_file);   // loads tokens.txt immediately

    bool parseTokens(const std::string &input_file);  // reload the token rules
    bool parse(const std::string &input_file);        // lex a file
    bool parseString(const std::string &input);       // lex an in-memory string

    std::vector<Token> &getTokens();                  // the last lex's output
};
```

* The constructor reads and compiles `tokens.txt`. If that throws (missing file,
  bad pattern), the error is logged and every later `parse*` call returns `false`.
* `parseString()` **clears the token buffer first**, so a REPL can call it once
  per line without accumulating (that is exactly what `examples/test` does).
* `parse(file)` reads the file and delegates to `parseString`.
* Both return `false` on an unlexable character and log
  `Invalid token on line: R, column: C`. **Check the return value** — the examples
  that ignore it end up parsing a truncated token stream.
* `getTokens()` returns a reference to the live buffer, including ignored tokens.
  Filter them yourself before constructing the `Engine`.

### `Parsing::Tokenizer::FileParser` — `Tokenizer/FileParser.h`

The `tokens.txt` reader. You rarely touch it directly, but it is public:

```cpp
struct TokenMaper { std::string tokenName; Utils::Regex::Matcher regex; bool ignore = false; };
using TokenMap = std::vector<TokenMaper>;

class FileParser
{
public:
    FileParser() = default;
    explicit FileParser(const std::string &file_path);

    bool parse();                 // throws std::runtime_error if the file is missing
    void print();                 // colourised dump of the token rules
    TokenMap &getTokenMaps();
};
```

`print()` is handy when a token rule is not firing:

```
 ==== INPUT TOKENS ====
'NUM': ([1-9][0-9]* | '0'*)
'WHITESPACE': (' ' | \n)+ [IGNORED]
```

### `Parsing::Syntax::Symbol` / `Rule` / `Grammar` — `Syntax/Grammar.h`

The in-memory grammar. A `Symbol` is one node of a rule's right-hand side:

```cpp
enum class SymKind
{
    Terminal,  // matches a token by NAME   -> text = token name
    Literal,   // matches a token by VALUE  -> text = literal text
    RuleRef,   // matches another rule      -> text = rule name
    Seq,       // a b c    : all children, in order
    Choice,    // a | b    : first child that matches
    Star,      // a*       : zero or more of children[0]
    Plus,      // a+       : one or more of children[0]
    Optional,  // a?       : zero or one of children[0]
};

struct Symbol { SymKind kind; std::string text; std::vector<Symbol> children; };
struct Rule   { std::string name; Symbol body; };

struct Grammar
{
    std::unordered_map<std::string, Rule> rules;
    std::string startRule;                     // the first rule declared in the file

    bool has(const std::string &name) const;
    std::string validate() const;              // "" on success, else the bad rule name
};
```

Free helpers let you build a grammar **without any `.syn` file**:

```cpp
using namespace Parsing::Syntax;

Grammar g;
g.startRule = "pair";
g.rules["pair"] = Rule{"pair", seq({terminal("NUM"), literal(","), terminal("NUM")})};
// also: ruleRef(), choice({...}), star(s), plus(s), optional(s)
```

### `Parsing::Syntax::GrammarParser` — `Syntax/GrammarParser.h`

```cpp
class GrammarParser
{
public:
    static Grammar parseFile(const std::string &path);
    static Grammar parseText(const std::string &text);
};
```

Both throw `std::runtime_error` on a malformed grammar (`missing ')'`,
`rule not terminated with ';'`, `reference to undefined rule 'x'`, …).
`parseText` is useful for tests and for embedding a grammar in a string literal.

### `Parsing::Syntax::Node` — `Syntax/Node.h`

The homogeneous CST node. Every node has the same type; you dispatch on strings.

```cpp
struct Node
{
    std::string rule;                        // rule name — empty on leaves
    std::optional<Tokenizer::Token> token;   // set on leaves only
    std::vector<Node> kids;

    bool isLeaf() const;                     // token.has_value()
    std::string text() const;                // token->value on leaves, rule otherwise
    bool isTokenName(const std::string &n) const;   // leaf && token->name == n
};

void printTree(const Node &node, int depth = 0);   // colourised debug dump
```

`printTree` is your main debugging tool — see the next section for its output.

### `Parsing::Syntax::Engine` — `Syntax/Engine.h`

```cpp
class Engine
{
public:
    Engine(const Grammar &grammar, std::vector<Tokenizer::Token> tokens);

    std::optional<Node> parse(const std::string &startRule);

    std::size_t furthestPos() const;              // furthest token index reached
    const Tokenizer::Token *furthestToken() const;// nullptr if that is past the end
};
```

* Holds a **reference** to the grammar (it must outlive the engine) and **owns a
  copy** of the token vector.
* `parse()` returns `nullopt` unless the start rule matches *and* every token is
  consumed.
* On failure, `furthestToken()` gives you the best available error location: the
  furthest point any alternative reached. `nullptr` there means "ran out of
  input". The idiomatic error report:

  ```cpp
  if (const auto *t = engine.furthestToken())
      std::cerr << "parse error near " << t->name << " '" << t->value << "'\n";
  else
      std::cerr << "parse error: unexpected end of input\n";
  ```
* An `Engine` is single-shot per input in practice: construct a fresh one per line
  in a REPL (the grammar object is reused).

### `Parsing::Syntax::Visitor<R>` — `Syntax/Visitor.h`

A generic visitor over the **CST** (not the generated AST). Use it when the parse
tree is simple enough that lowering to a typed AST is not worth it.

```cpp
template <typename R>
class Visitor
{
public:
    R visit(const Node &node);          // leaf ? visitTerminal : visitRule

protected:
    virtual R visitTerminal(const Node &node) = 0;
    virtual R visitRule(const Node &node) = 0;
    R visitOnly(const Node &node);      // visit(node.kids.back()) — pass-through rules
};
```

`src/Syntax/Evaluator.h` is a complete implementation for the arithmetic grammar:
`visitRule` switches on `node.rule`, and `foldBinary` folds
`[operand, OP, operand, OP, …]` left-to-right.

---

## The parse tree: how nodes are shaped

This is the single most important thing to understand before writing a builder,
because your builder reads `node.kids` **by position**.

**Flattening rules** — how each `SymKind` contributes to the enclosing node:

| Symbol kind                | Effect on the parent node's `kids`                   |
|----------------------------|-------------------------------------------------------|
| `Terminal`, `Literal`      | appends **one leaf** carrying the matched token        |
| `RuleRef`                  | appends **one child node** with `rule = <name>`        |
| `Seq`                      | **flattens** — its items append directly to the parent |
| `( … )` grouping           | **flattens** — grouping creates no node                |
| `Choice`                   | **flattens** — only the winning alternative appears    |
| `Star` / `Plus`            | **flattens** — each repetition appends in place        |
| `Optional`                 | **flattens** — contributes nothing when it doesn't match |

In short: **only rule references create nodes; everything else flattens into the
current rule's node.** So a rule's `kids` is exactly the flat left-to-right list
of the terminals and rule references its body matched.

### Worked example 1

Grammar (`examples/basic/lang.syn`), input `1 + 2 * 3`:

```
entry
 expr
  plusexpr
   mulexpr
    atom
     [NUM] '1'
  n_plusexpr
   [PLUS] '+'
   plusexpr
    mulexpr
     atom
      [NUM] '2'
    n_mulexpr
     [MUL] '*'
     mulexpr
      atom
       [NUM] '3'
```

Read it against the rules:

* `expr : plusexpr n_plusexpr?` → `kids = [plusexpr, n_plusexpr]`, or just
  `[plusexpr]` when the optional tail is absent. That size check is how the
  builder knows whether there is a tail.
* `n_plusexpr : (PLUS | MINUS) plusexpr n_plusexpr?` → `kids = [OP-leaf, plusexpr]`
  or `[OP-leaf, plusexpr, n_plusexpr]`. The `( … )` and the `|` left no trace —
  only the winning `PLUS` leaf is there.
* `atom : NUM | group` → `kids = [ NUM-leaf ]` or `kids = [ group-node ]`. You
  distinguish them with `kids[0].isTokenName("NUM")` vs `kids[0].rule == "group"`.

### Worked example 2

Grammar (`examples/test/lang.syn`), input `group 1 + 5 thru 8 at 100`:

```
entry
 command
  selection
   sel
    grpsel
     [GROUP] 'group'
     [NUM] '1'
   op
    [PLUS] '+'
   sel
    fixsel
     [NUM] '5'
     [THRU] 'thru'
     [NUM] '8'
  at
   [AT] 'at'
   [NUM] '100'
```

* `selection : sel (op sel)*` → `kids = [sel, op, sel, op, sel, …]`. The star
  flattened; there is no wrapper node per repetition. Walk the list in pairs.
* `command : selection at? | store | delete | clear | at` → `kids = [selection, at]`
  here. Check `kids[0].rule` to find out which alternative won, and
  `kids.size() == 2` to detect the optional trailing `at`.
* `fixsel : NUM (THRU NUM)?` → `kids.size()` is 1 for a single fixture, 3 for a
  range.

### Practical consequences

* **Size checks are how you read optionals.** `if (v.kids.size() > 2 && v.kids[2].isTokenName("EQ"))`.
* **Scan by rule name when a star mixes kinds.** `examples/complex` does
  `for (const Node &k : s.kids) if (k.rule == "var_decl") …` rather than indexing.
* **Add a wrapper rule if the flat layout is awkward.** Introducing
  `group : LPAREN expr RPAREN` instead of inlining `( expr )` gives you a node to
  hang code on. That is why the example grammars have `group`, `op`, `sel`, etc.
* Dump the tree whenever a builder surprises you:
  `Parsing::Syntax::printTree(*cst);`

---

## Lowering: CST → typed AST

There is no automatic CST→AST mapping; you write one small function per rule.
This is deliberate — it is where you drop noise tokens, fold associativity, and
collapse pass-through rules.

The house style (`examples/*/…Builder.cpp`) is: **one static function per grammar
rule, with the rule and its kid layout in a comment above it.**

```cpp
// grpsel : GROUP NUM              kids = [GROUP, NUM]
static std::unique_ptr<Group> buildGrpsel(const Node &g)
{
    auto n = std::make_unique<Group>();
    n->id = intOf(g.kids[1]);
    return n;
}

// fixsel : NUM (THRU NUM)?        kids = [NUM] | [NUM, THRU, NUM]
static SelectorPtr buildFixsel(const Node &f)
{
    if (f.kids.size() == 1)
    {
        auto n = std::make_unique<Fixture>();
        n->id = intOf(f.kids[0]);
        return n;
    }
    auto n = std::make_unique<FixtureRange>();
    n->from = intOf(f.kids[0]);
    n->to   = intOf(f.kids[2]);
    return n;
}

// sel : fixsel | grpsel           kids = [ chosen ]
static SelectorPtr buildSel(const Node &sel)
{
    const Node &c = sel.kids[0];
    if (c.rule == "fixsel") return buildFixsel(c);
    if (c.rule == "grpsel") return buildGrpsel(c);
    throw std::runtime_error("buildSel: unexpected '" + c.rule + "'");
}
```

Three recurring patterns:

**Choice** — look at `kids[0].rule` (or `isTokenName` for a terminal alternative)
and dispatch. Always `throw` in the default branch; it turns a grammar/builder
mismatch into a clear message instead of a silent wrong tree.

**Optional** — check `kids.size()`.

**Right-leaning tail → left-associative fold.** The `head tail?` idiom parses
`1 - 2 - 3` as `1 - (2 - 3)`. Fold it left while building:

```cpp
// tail : (OP operand) tail?   -> a left-associative BinaryExpr chain
static Basic::ExprPtr foldTail(Basic::ExprPtr lhs, const Node &tail)
{
    auto bin = std::make_unique<Basic::BinaryExpr>();
    bin->op  = tail.kids[0].token->value;      // the PLUS/MINUS/MUL/DIV leaf
    bin->lhs = std::move(lhs);
    bin->rhs = buildExpr(tail.kids[1]);
    return tail.kids.size() == 3 ? foldTail(std::move(bin), tail.kids[2])
                                 : std::move(bin);
}
```

**Pass-through rules** (`mulexpr : atom`) just recurse into `kids[0]` and produce
no node of their own — one of the main reasons to lower at all.

---

## Walking the AST: visitors and interpreters

For every `category X`, the generator emits `struct XVisitor` with one pure
`visit(Node &)` per member, and `X::accept(XVisitor &)`. An interpreter is a
struct that implements the interface:

```cpp
struct Interpreter : Sel::CommandVisitor
{
    Sel::Context ctx;                       // whatever state you need

    void execute(Sel::Program &prog)        // Program = vector<CommandPtr>
    {
        for (auto &cmd : prog)
            cmd->accept(*this);
    }

    void visit(Sel::SelectCmd &c) override
    {
        ctx.selected = foldSelection(c.items);
        if (c.hasAt) applyAt(c.at);
    }
    void visit(Sel::StoreCmd &c)  override { /* … */ }
    void visit(Sel::DeleteCmd &c) override { /* … */ }
    void visit(Sel::ClearCmd &)   override { ctx.selected.clear(); }
    void visit(Sel::AtCmd &c)     override { applyAt(c.at); }
};
```

Because visitors return `void`, the usual way to produce a value is a member on
the visitor that `visit` writes and a thin wrapper that reads it back:

```cpp
struct Evaluator : Basic::ExprVisitor
{
    long long result = 0;

    long long eval(Basic::Expr &e) { e.accept(*this); return result; }

    void visit(Basic::NumberExpr &n) override { result = n.value; }
    void visit(Basic::BinaryExpr &b) override
    {
        long long lhs = eval(*b.lhs);       // recursion via the wrapper
        long long rhs = eval(*b.rhs);
        if      (b.op == "+") result = lhs + rhs;
        else if (b.op == "-") result = lhs - rhs;
        else if (b.op == "*") result = lhs * rhs;
        else if (b.op == "/") result = lhs / rhs;
        else throw std::runtime_error("unknown operator '" + b.op + "'");
    }
};
```

**One visitor per category, one pass per concern.** `examples/test/Tree/` is the
model:

| File                 | Role                                                          |
|----------------------|---------------------------------------------------------------|
| `Builder.{h,cpp}`    | CST → `Sel::` typed AST                                       |
| `Context.{h,cpp}`    | interpreter state (selection, groups, presets, levels)        |
| `Resolver.{h,cpp}`   | a `SelectorVisitor` — expands one selector to a set of ids    |
| `Interpreter.{h,cpp}`| a `CommandVisitor` — executes commands against the `Context`  |
| `All.h`              | study file: the whole thing inlined in dependency order, not built |

Adding a new pass (a pretty-printer, a type checker) means one more struct
implementing the same interface — no changes to the generated header.

---

## The examples

### `examples/basic` — arithmetic evaluator

The minimal end-to-end path, ~110 lines of `main.cpp` covering all five stages:
lex, parse, lower with `buildExpr`/`foldTail`, evaluate with an `ExprVisitor`.
Reads `input.txt`, prints `result = 3`. **Start here.**

### `examples/complex` — a C-like language

A much larger grammar: functions, structs, pointer types, member access chains
(`p.d.a`), calls, compound statements, return statements. Demonstrates a real
`AstBuilder` class (`AstBuilder.{h,cpp}`, one `build*` method per rule) and a
hand-written `Ast.h` with a virtual `print()`. It parses `input.txt` and prints
the typed AST; there is no evaluator.

Note this example keeps its own hand-written `Ast.h` **and** has an `ast.spec`
that CMake still runs the generator on — the checked-in header is what the code
actually includes.

### `examples/test` — interactive lighting console

The most complete example, and the one that shows the intended production shape:

* token rules and grammar are loaded **once**, outside the loop;
* a single `Interpreter` (and its `Context`) persists across lines, so
  `store group 1` on one line and `at 100` on the next share state;
* each line is lexed from memory with `parseString`, parsed by a **fresh
  `Engine`**, lowered, and executed;
* parse errors print and the loop continues;
* data files are located relative to the executable via `/proc/self/exe`.

```
$ ./test_example
fixture console — type commands, 'state' to inspect, 'quit' to exit
> 1 thru 10
select {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
> at 100
  at 100 -> {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
> store group 1
store group 1 <- {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
> clear
> group 1 + 15
select {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15}
> state
=== final state ===
selection: {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15}
groups:
  group 1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
```

Its `Ast.gen.h` is **checked in** rather than generated at build time, so its
`CMakeLists.txt` is hand-written instead of using `add_synparser_example()`.
Regenerate it manually after editing `ast.spec`.

---

## Using synparser in your own project

### As a subdirectory

```cmake
add_subdirectory(path/to/NewSyntaxParser)

add_executable(mylang main.cpp)
target_link_libraries(mylang PRIVATE synparser)   # brings src/ onto the include path
```

`synparser` is a `STATIC` library that links `utils` `PUBLIC`, so the Utils
headers (`Utils/Logging/Logger.h`, `Utils/Regex/Matcher.h`, …) are available too.

### The `add_synparser_example()` helper

Defined in the top-level `CMakeLists.txt`. Call it from a directory that contains
`ast.spec`, `tokens.txt`, `lang.syn`, `input.txt` and one or more `.cpp` files:

```cmake
add_synparser_example(mylang)
```

It:

1. runs `gen_ast.py` on `ast.spec` → `${CMAKE_CURRENT_BINARY_DIR}/gen/Ast.gen.h`,
   re-running whenever the spec or the generator changes;
2. globs `*.cpp` in the directory into an executable named `mylang`;
3. links `synparser` and puts both the generated dir and the source dir on the
   include path (so `#include "Ast.gen.h"` just works);
4. copies `tokens.txt`, `input.txt` and `lang.syn` next to the binary after each
   build.

Then register the directory in the top-level file:

```cmake
add_subdirectory(examples/mylang)
```

### Generating the AST header outside CMake

```sh
python3 generator/gen_ast.py path/to/ast.spec path/to/Ast.gen.h
```

Exit code 2 on bad arguments; a `SyntaxError` traceback on a malformed spec.

### Logging

The library logs through `Utils::Logger`. Debug output (every token match, etc.)
is off by default; turn it on with:

```cpp
#include "Utils/Logging/Logger.h"
Utils::Logger::setLevel(Utils::Logger::DEBUGGING);
```

---

## Editor support

`editors/vscode/` is a dependency-free TextMate extension highlighting all three
input formats:

| Format   | Files                    | Scope                         |
|----------|--------------------------|-------------------------------|
| Grammar  | `*.syn`                  | `source.syntaxparser.syn`     |
| AST spec | `ast.spec`, `*.spec`     | `source.syntaxparser.astspec` |
| Tokens   | `tokens.txt`, `*.tokens` | `source.syntaxparser.tokens`  |

```sh
ln -s "$(pwd)/editors/vscode" ~/.vscode/extensions/syntaxparser-highlight
```

Then reload VSCode. See `editors/vscode/README.md` for details.

---

## Limitations and gotchas

**Grammar**

* **No left recursion.** `expr : expr OP term | term ;` overflows the stack and
  crashes. Use `head tail?` or `*`/`+`.
* **Ordered choice, no re-try.** Put longer alternatives first;
  `NUM | NUM PLUS NUM` will never match `1 + 2`.
* **Greedy, non-backtracking repetition.** `a* a` can never match.
* **Full-line comments only.** `#` must be the first non-space character; an
  inline `# …` after a rule is a syntax error.
* **Every token must be consumed** or the parse fails.
* **No parse-time semantic actions**, no error recovery, and no ambiguity
  reporting. Failure gives you one position (`furthestToken()`), not a diagnosis.
* No memoization — a pathological grammar can backtrack exponentially. The
  example grammars are far from that, but keep alternatives cheap to reject.

**Lexer**

* **First match wins, in file order** — not longest match. Keywords before
  `IDENTIFIER = \T+`, always.
* **Ignored tokens are still emitted.** `!` only sets `Token::ignore`; you must
  filter before constructing the `Engine`.
* **Tabs are not whitespace** unless you add them; the shipped rule is
  `(' ' | \n)+`. There is no `\t` escape — use a literal tab inside `'…'`.
* **`Token::row` / `Token::col` are unreliable.** The lexer source marks the
  offset computation `// THIS IS WRONG`, and lex errors report `line: 0,
  column: 0`. Use `Token::start` / `Token::end` (byte offsets) if you need
  positions today.
* **Check the return value of `parse()` / `parseString()`.** On a lex error they
  return `false` and leave a truncated token stream behind; ignoring the result
  produces a confusing downstream parse error instead of the real one.
* Avoid patterns that can match the empty string.
* `FileParser::parse()` always returns `false` on success — it signals failure by
  throwing. Do not test its return value; `Parser::parseTokens` handles it.

**AST generation**

* `Ast.gen.h` is **auto-generated — never hand-edit it.** Edit `ast.spec` and
  rebuild (or re-run `gen_ast.py` for `examples/test`, whose header is checked in).
* Generated nodes are aggregates with no constructors: `make_unique<T>()` then
  assign fields.
* Category fields are `unique_ptr`, so AST nodes are move-only. Pass them by
  reference into visitors, `std::move` them into parents.
* Field ordering in the spec is the field ordering in the struct; nothing else
  about the spec is validated, and an unknown type name is silently emitted
  verbatim as a C++ type.

**Runtime**

* `Engine` holds a **reference** to the `Grammar` — keep the grammar alive.
* Reuse the `Grammar` across inputs; construct a new `Engine` per input.
* Example binaries other than `examples/test` resolve data files relative to the
  **current working directory**, so run them from their build directory.
