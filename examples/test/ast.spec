# =============================================================================
#  ast.spec — AST for the fixture/group command grammar (lang.syn).
#  Consumed by gen_ast.py -> Ast.gen.h.
#
#    entry     : command+                           -> a sequence of Command
#    command   : selection at? | store | delete
#              | clear | at
#    selection : sel (op sel)*                      -> SelectCmd (list of Item)
#    sel       : fixsel | grpsel                    -> Fixture|FixtureRange|Group
#    modsel    : grpsel | presetsel                 -> Group|Preset
#    fixsel    : NUM (THRU NUM)?
#    grpsel    : GROUP NUM
#    presetsel : PRESET NUM DOT NUM
#    store     : STORE modsel
#    delete    : DELETE modsel
#    clear     : CLEAR
#    at        : AT (NUM | presetsel)
# =============================================================================

namespace Sel

# A program is a sequence of commands.
program Command[]

# ---- category: the selectable / addressable objects ----------------------
category Selector {
    Fixture      { i64 id }               # fixsel single    e.g. 13
    FixtureRange { i64 from; i64 to }      # fixsel range     e.g. 1 thru 10
    Group        { i64 id }               # grpsel           e.g. group 4
    Preset       { i64 bank; i64 number } # presetsel        e.g. preset 2.5
}

# ---- record: the value an 'at' applies -----------------------------------
#   at NUM        -> isPreset = false, level holds the number
#   at preset a.b -> isPreset = true,  preset holds a Preset
record AtValue {
    bool     isPreset
    f64      level
    Selector preset
}

# ---- category: the top-level commands ------------------------------------
category Command {
    SelectCmd { Item[] items; bool hasAt; AtValue at }  # selection at?
    StoreCmd  { Selector target }                       # store modsel
    DeleteCmd { Selector target }                       # delete modsel
    ClearCmd  { }                                       # clear
    AtCmd     { AtValue at }                             # bare at
}

# ---- record: pairs each selector with the operator that precedes it ------
#   op is "+" / "-"  (empty for the first item, which has no leading op)
record Item {
    string   op
    Selector sel
}
