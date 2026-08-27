# SyntaxParser Highlighting (VSCode)

Syntax highlighting for the three input formats consumed by `NewSyntaxParser`.
Pure TextMate grammars — **no runtime dependencies**, no build step.

| Format      | Files                       | Grammar scope                     |
|-------------|-----------------------------|-----------------------------------|
| Grammar     | `*.syn`                     | `source.syntaxparser.syn`         |
| AST spec    | `ast.spec`, `*.spec`        | `source.syntaxparser.astspec`     |
| Tokens      | `tokens.txt`, `*.tokens`    | `source.syntaxparser.tokens`      |

## Install (development)

Symlink or copy this folder into your VSCode extensions dir, then reload:

```sh
ln -s "$(pwd)" ~/.vscode/extensions/syntaxparser-highlight
```

Or open this folder in VSCode and press `F5` to launch an Extension Development Host.

## What gets highlighted

- **tokens** — token names (`!`-prefixed = skipped), `=`, single-quoted
  literals, `\`-escapes, `[...]` char classes, and the regex operators
  `| * + ? ( )`.
- **.syn** — rule definitions (name `:` … `;`), UPPERCASE terminal references,
  lowercase rule references, and the EBNF operators `| * + ? ( )`.
- **ast.spec** — the `namespace` / `program` / `category` / `record` keywords,
  the `string` / `i64` primitives, capitalised type references (with `[]`
  lists), field names, and `{ } ;` punctuation.

All three treat `#` as a line comment.
