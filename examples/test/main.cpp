#include "Syntax/Engine.h"
#include "Syntax/GrammarParser.h"
#include "../../include/SyntaxParser/Tokenizer/Parser.h"

#include "Tree/Builder.h"
#include "Tree/Interpreter.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

// Directory containing the running executable, so data files (tokens.txt,
// lang.syn) are found regardless of the current working directory. Prefers
// /proc/self/exe (Linux), falling back to argv[0].
static fs::path exeDir(const char *argv0) {
  std::error_code ec;
  fs::path self = fs::read_symlink("/proc/self/exe", ec);
  if (!ec)
    return self.parent_path();
  fs::path a(argv0);
  return a.has_parent_path() ? a.parent_path() : fs::current_path();
}

// Interactive fixture console.
//   - Token rules and grammar are loaded ONCE.
//   - A single Interpreter (and its Context) lives across all lines, so
//     `store group 1` on one line and `at 100` on the next share state.
//   - Each line is lexed -> parsed -> lowered -> executed; parse errors just
//     print and the loop continues.
//
// Type 'state' to dump the console, 'quit' / Ctrl-D to exit.
int main(int argc, char **argv) {
  const fs::path base = exeDir(argc > 0 ? argv[0] : "");
  Parsing::Tokenizer::Parser lexer((base / "tokens.txt").string());
  auto g = Parsing::Syntax::GrammarParser::parseFile("lang.syn");
  if (!g.has_value())
    return 1;

  auto grammar = g.value();
  // auto grammar = Parsing::Syntax::GrammarParser::parseFile((base / "lang.syn").string());
  Sel::Interpreter interp;

  std::string line;
  std::cout << "fixture console — type commands, 'state' to inspect, 'quit' to exit\n> ";
  while (std::getline(std::cin, line)) {
    if (line.empty()) {
      std::cout << "> ";
      continue;
    }
    if (line == "quit" || line == "exit")
      break;
    if (line == "state") {
      interp.dump();
      std::cout << "> ";
      continue;
    }

    // 1. Lex this line from memory.
    if (!lexer.parseString(line)) {
      std::cerr << "  lex error\n> ";
      continue;
    }
    std::vector<Parsing::Tokenizer::Token> tokens;
    for (auto &t : lexer.getTokens())
      if (!t.ignore)
        tokens.push_back(t);

    // 2. Parse against the grammar (fresh engine per line).
    Parsing::Syntax::Engine engine(grammar, tokens);
    auto cst = engine.parse(grammar.startRule);
    if (!cst) {
      if (const auto *t = engine.furthestToken())
        std::cerr << "  parse error near " << t->name << " '" << t->value << "'\n";
      else
        std::cerr << "  parse error: unexpected end of input\n";
      std::cout << "> ";
      continue;
    }

    // 3. Lower to the typed AST and 4. execute against the persistent Context.
    Sel::Program program = Sel::buildProgram(*cst);
    interp.execute(program);
    std::cout << "> ";
  }

  std::cout << "\n";
  interp.dump();
  return 0;
}
