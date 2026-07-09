#!/usr/bin/env python3
# =============================================================================
#  gen_ast.py — generate a visitor-pattern AST header from an .spec file.
#
#  Usage:
#     gen_ast.py <input.spec> <output.h>
#
#  The generator is fully data-driven: it knows nothing about any particular
#  grammar. It reads whatever namespace / categories / nodes / records the spec
#  declares and emits, per category, a base class, a Visitor interface, and one
#  concrete struct per node with an accept() that double-dispatches.
#
#  Spec grammar (see ast.spec for a worked example):
#     namespace <Name>
#     program   <Type>                     # optional: emits `using Program = ...`
#     record    <Name> { <field>* }        # plain struct, never visited
#     category  <Name> { <Node> { <field>* } ... }
#
#     <field> : <Type> <name>              # separated by ';' or newlines
#     <Type>  : <base> | <base>[]          # '[]' -> std::vector<...>
#
#  Field <base> resolves in this order:
#     a builtin (see BUILTINS)  |  a category name -> <Name>Ptr
#     a record name -> by value |  anything else -> emitted verbatim, by value
# =============================================================================

import re
import sys

# base name -> (C++ type, scalar default or None). Extend freely; the rest of
# the generator does not care what is in here.
BUILTINS = {
    "string": ("std::string", None),
    "bool": ("bool", "false"),
    "i32": ("int", "0"),
    "i64": ("long long", "0"),
    "u32": ("unsigned", "0"),
    "u64": ("unsigned long long", "0"),
    "f32": ("float", "0"),
    "f64": ("double", "0"),
}

Field = tuple  # (base: str, is_array: bool, name: str)


class Spec:
    def __init__(self):
        self.namespace = None
        self.program = None            # (base, is_array) or None
        self.records = []              # [(name, [Field])]
        self.categories = []           # [(name, [(node_name, [Field])])]

    @property
    def category_names(self):
        return {name for name, _ in self.categories}

    @property
    def record_names(self):
        return {name for name, _ in self.records}


# ---- tokenizer --------------------------------------------------------------
_TOKEN = re.compile(r"\[\]|[{};]|[A-Za-z_][A-Za-z0-9_]*")


def tokenize(text):
    # Strip full-line and inline '#' comments (the spec has no string literals).
    stripped = "\n".join(line.split("#", 1)[0] for line in text.splitlines())
    return _TOKEN.findall(stripped)


# ---- recursive-descent spec parser -----------------------------------------
class Parser:
    def __init__(self, tokens):
        self.toks = tokens
        self.i = 0

    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else None

    def next(self):
        t = self.peek()
        if t is None:
            raise SyntaxError("unexpected end of spec")
        self.i += 1
        return t

    def expect(self, tok):
        t = self.next()
        if t != tok:
            raise SyntaxError(f"expected '{tok}', got '{t}'")
        return t

    def parse(self):
        spec = Spec()
        while self.peek() is not None:
            kw = self.next()
            if kw == "namespace":
                spec.namespace = self.next()
            elif kw == "program":
                spec.program = self.parse_type()
            elif kw == "record":
                name = self.next()
                spec.records.append((name, self.parse_block_fields()))
            elif kw == "category":
                name = self.next()
                spec.categories.append((name, self.parse_category_body()))
            else:
                raise SyntaxError(f"unknown top-level keyword '{kw}'")
        if spec.namespace is None:
            raise SyntaxError("spec is missing a 'namespace' declaration")
        return spec

    def parse_type(self):
        base = self.next()
        is_array = False
        if self.peek() == "[]":
            self.next()
            is_array = True
        return (base, is_array)

    def parse_block_fields(self):
        self.expect("{")
        fields = []
        while self.peek() != "}":
            base, is_array = self.parse_type()
            name = self.next()
            fields.append((base, is_array, name))
            if self.peek() == ";":
                self.next()
        self.expect("}")
        return fields

    def parse_category_body(self):
        self.expect("{")
        nodes = []
        while self.peek() != "}":
            node_name = self.next()
            nodes.append((node_name, self.parse_block_fields()))
        self.expect("}")
        return nodes


# ---- C++ rendering ----------------------------------------------------------
def render_field(spec, field):
    base, is_array, name = field
    if base in BUILTINS:
        cpp, default = BUILTINS[base]
    elif base in spec.category_names:
        cpp, default = base + "Ptr", None
    else:  # record or verbatim C++ type -> stored by value
        cpp, default = base, None
    if is_array:
        cpp, default = f"std::vector<{cpp}>", None
    suffix = f" = {default}" if default is not None else ""
    return f"{cpp} {name}{suffix};"


def render_program(spec):
    base, is_array = spec.program
    if base in spec.category_names:
        elem = base + "Ptr"
    else:
        elem = base
    return f"std::vector<{elem}>" if is_array else elem


def generate(spec):
    ns = spec.namespace
    out = []
    w = out.append

    w("#pragma once")
    w("// AUTO-GENERATED from ast.spec by gen_ast.py — do not edit by hand.")
    w("#include <memory>")
    w("#include <string>")
    w("#include <vector>")
    w("")
    w(f"namespace {ns}")
    w("{")

    # forward declarations (concrete nodes only, in category/declaration order)
    w("    // ---- forward declarations ----")
    for _, nodes in spec.categories:
        for node_name, _ in nodes:
            w(f"    struct {node_name};")
    w("")

    # visitor interfaces (one per category)
    w("    // ---- visitor interfaces (one per category) ----")
    for idx, (cat, nodes) in enumerate(spec.categories):
        if idx:
            w("")
        w(f"    struct {cat}Visitor")
        w("    {")
        w(f"        virtual ~{cat}Visitor() = default;")
        for node_name, _ in nodes:
            w(f"        virtual void visit({node_name} &) = 0;")
        w("    };")
    w("")

    # category bases + smart-pointer aliases
    w("    // ---- category bases + smart-pointer aliases ----")
    for idx, (cat, _) in enumerate(spec.categories):
        if idx:
            w("")
        w(f"    struct {cat}")
        w("    {")
        w(f"        virtual ~{cat}() = default;")
        w(f"        virtual void accept({cat}Visitor &v) = 0;")
        w("    };")
        w(f"    using {cat}Ptr = std::unique_ptr<{cat}>;")
    w("")

    # plain records
    if spec.records:
        w("    // ---- records (plain structs, not visited) ----")
        for idx, (name, fields) in enumerate(spec.records):
            if idx:
                w("")
            w(f"    struct {name}")
            w("    {")
            for f in fields:
                w(f"        {render_field(spec, f)}")
            w("    };")
        w("")

    # concrete nodes
    w("    // ---- concrete nodes ----")
    first = True
    for cat, nodes in spec.categories:
        for node_name, fields in nodes:
            if not first:
                w("")
            first = False
            w(f"    struct {node_name} : {cat}")
            w("    {")
            for f in fields:
                w(f"        {render_field(spec, f)}")
            w(f"        void accept({cat}Visitor &v) override {{ v.visit(*this); }}")
            w("    };")

    # program root alias
    if spec.program is not None:
        w("")
        w(f"    using Program = {render_program(spec)};")

    w("}")
    return "\n".join(out) + "\n"


def main(argv):
    if len(argv) != 3:
        sys.stderr.write(f"usage: {argv[0]} <input.spec> <output.h>\n")
        return 2
    in_path, out_path = argv[1], argv[2]
    with open(in_path, "r", encoding="utf-8") as f:
        spec = Parser(tokenize(f.read())).parse()
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(generate(spec))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
