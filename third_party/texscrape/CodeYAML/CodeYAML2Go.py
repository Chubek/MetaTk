#!/usr/bin/env python3
"""
CodeYAML2Go.py — Compile TeX.yaml (Pascal-W AST in YAML) into TeX.go

Produces compilable Go from the Concrete-AST-PascalW YAML representation.
Handles WEB macros as Go stubs, Pascal types as Go types,
goto labels as Go labels, var-params as pointers, etc.
"""

import sys
import yaml

# ──────────────────────────────────────────────────────────────────
# Identifier safety: Pascal names that collide with Go keywords/builtins
# ──────────────────────────────────────────────────────────────────

# Go reserved words — cannot be used as identifiers at all.
GO_RESERVED = {
    "break", "case", "chan", "const", "continue", "default", "defer", "else",
    "fallthrough", "for", "func", "go", "goto", "if", "import", "interface",
    "map", "package", "range", "return", "select", "struct", "switch", "type",
    "var",
}

# Go predeclared identifiers — usable but shadowing them is fragile/risky,
# so we mangle subprogram names that collide (variables are left alone to
# avoid touching every identifier site).
GO_BUILTINS = {
    "append", "bool", "byte", "cap", "clear", "close", "complex", "copy",
    "delete", "error", "false", "float32", "float64", "imag", "int", "int8",
    "int16", "int32", "int64", "iota", "len", "make", "max", "min", "new",
    "nil", "print", "println", "real", "recover", "rune", "string", "true",
    "uint", "uint8", "uint16", "uint32", "uint64", "uintptr", "any",
    "comparable",
}

def go_safe_name(name):
    """Mangle a Pascal subprogram name to avoid Go keyword/builtin clashes.
    Applied to function definitions and call sites (not plain variables)."""
    if name in GO_RESERVED or name in GO_BUILTINS:
        return name + "_"
    return name


# ──────────────────────────────────────────────────────────────────
# Pascal standard library: functions and procedures mapped to Go
# ──────────────────────────────────────────────────────────────────

# Standard functions: name → lambda(arg_strs) -> Go expression string.
STD_FUNCS = {
    "abs":   lambda a: f"abs_({a[0]})" if a else "abs_()",
    "sqr":   lambda a: f"sqr_({a[0]})" if a else "sqr_()",
    "odd":   lambda a: f"(({a[0]}&1)!=0)" if a else "false",
    "round": lambda a: f"round_({a[0]})" if a else "round_()",
    "trunc": lambda a: f"trunc_({a[0]})" if a else "trunc_()",
    "chr":   lambda a: f"byte({a[0]})" if a else "byte(0)",
    "ord":   lambda a: f"int({a[0]})" if a else "0",
    "succ":  lambda a: f"({a[0]}+1)" if a else "1",
    "pred":  lambda a: f"({a[0]}-1)" if a else "-1",
    "eof":   lambda a: "eof_()" if not a else f"eof_({a[0]})",
    "eoln":  lambda a: "eoln_()" if not a else f"eoln_({a[0]})",
    "max":   lambda a: f"max_({', '.join(a)})",
    "min":   lambda a: f"min_({', '.join(a)})",
    "length":lambda a: f"len({a[0]})" if a else "0",
    "copy":  lambda a: f"copy_({', '.join(a)})",
    "concat":lambda a: f"concat_({', '.join(a)})",
    "pos":   lambda a: f"pos_({', '.join(a)})",
    "upcase":lambda a: f"upcase_({a[0]})" if a else "0",
    "lo":    lambda a: f"lo_({a[0]})" if a else "0",
    "hi":    lambda a: f"hi_({a[0]})" if a else "0",
    "mem":   lambda a: f"mem_({', '.join(a)})",
}

# Standard procedures: emitted as name_(...) calls to stub functions.
STD_PROCS = {
    "get", "put", "reset", "rewrite", "read", "readln",
    "write", "writeln", "page", "break", "pack", "unpack",
    "dispose", "new", "seek", "close", "flush",
}


def gen_call(name, arg_strs):
    """Render a call expression, mapping Pascal standard functions to Go."""
    f = STD_FUNCS.get(name)
    if f:
        return f(arg_strs)
    return f"{go_safe_name(name)}({', '.join(arg_strs)})"


def gen_proc_call(name, arg_strs, prefix):
    """Render a procedure call, mapping Pascal standard procedures to stubs."""
    if name in STD_PROCS:
        return f"{prefix}{name}_({', '.join(arg_strs)})\n"
    return f"{prefix}{go_safe_name(name)}({', '.join(arg_strs)})\n"


# ──────────────────────────────────────────────────────────────────
# Type mapping: Pascal → Go
# ──────────────────────────────────────────────────────────────────

INT_TYPES = {"integer", "scaled", "nonnegativeinteger", "smallnumber",
             "quarterword", "halfword", "twochoices", "fourchoices",
             "groupcode", "internalfontnumber", "fontindex",
             "dviindex", "hyphpointer", "glueord", "poolpointer",
             "strnumber"}

CHAR_TYPES = {"char", "ASCIIcode", "eightbits", "packedASCIIcode"}

FILE_TYPES = {"alphafile", "bytefile", "wordfile"}

RECORD_TYPES = {"twohalves", "fourquarters", "memoryword",
                "liststaterecord", "instaterecord"}

def pascal_type_to_go(type_obj, type_map):
    """Convert Pascal Type AST node to Go type string."""
    if type_obj is None:
        return "interface{}"
    kind = type_obj.get("kind")

    if kind == "NamedType":
        name = type_obj["name"]
        if name in INT_TYPES:
            return "int"
        if name in CHAR_TYPES:
            return "byte"
        if name == "real":
            return "float64"
        if name == "boolean":
            return "bool"
        if name == "string":
            return "string"
        if name in type_map:
            return type_map[name]
        return f"*{name}_t"

    if kind == "SubrangeType":
        return "int"

    if kind == "ArrayType":
        elem = pascal_type_to_go(type_obj.get("element_type"), type_map)
        return f"[]byte"  # Use byte slices for arrays

    if kind == "RecordType":
        return f"*{type_obj.get('_gen_name', 'record_t')}"

    if kind == "FileType":
        return "*os.File"

    if kind == "PointerType":
        base = pascal_type_to_go(type_obj.get("base"), type_map)
        return f"*{base}"

    return "interface{}"


# ──────────────────────────────────────────────────────────────────
# Expression generation
# ──────────────────────────────────────────────────────────────────

def gen_expr(expr, type_map, indent=0):
    """Generate Go expression code from an Expression AST node."""
    if expr is None:
        return ""
    kind = expr.get("kind")
    indent_str = "  " * indent

    if kind == "Literal":
        lt = expr.get("literal_type")
        val = expr.get("value")
        if lt == "integer":
            return str(val)
        if lt == "real":
            return f"{val:.10g}"
        if lt == "string":
            s = str(val).replace("\\", "\\\\").replace('"', '\\"')
            return f'"{s}"'
        if lt == "boolean":
            return "true" if val else "false"
        if lt == "nil":
            return "nil"
        return f"/* literal {lt}: {val} */"

    if kind == "Identifier":
        return expr["name"]

    if kind == "BinaryOp":
        op = expr["operator"]
        left = gen_expr(expr.get("left"), type_map, indent)
        right = gen_expr(expr.get("right"), type_map, indent)
        map_op = {"=": "==", "<>": "!=", "and": "&&", "or": "||",
                   "div": "/", "mod": "%", "in": "contains"}
        c_op = map_op.get(op, op)
        return f"({left} {c_op} {right})"

    if kind == "UnaryOp":
        op = expr["operator"]
        operand = gen_expr(expr.get("operand"), type_map, indent)
        if op == "not":
            return f"(!{operand})"
        if op == "@":
            return f"(&{operand})"
        return f"({op}{operand})"

    if kind == "Call":
        base_node = expr.get("base")
        if base_node and base_node.get("kind") == "Identifier":
            name = base_node["name"]
        else:
            name = gen_expr(base_node, type_map, indent)
        args = expr.get("arguments", [])
        arg_strs = [gen_expr_or_writefield(a, type_map, indent) for a in args]
        return gen_call(name, arg_strs)

    if kind == "Index":
        base = gen_expr(expr.get("base"), type_map, indent)
        indices = [gen_expr(idx, type_map, indent) for idx in expr.get("indices", [])]
        return f"{base}[{', '.join(indices)}]"

    if kind == "FieldAccess":
        base = gen_expr(expr.get("base"), type_map, indent)
        field = expr["field"]
        return f"{base}.{field}"

    if kind == "Deref":
        base = gen_expr(expr.get("base"), type_map, indent)
        return f"*{base}"

    if kind == "WriteField":
        return gen_expr(expr.get("value"), type_map, indent)

    return f"/* unknown expr kind: {kind} */"


def gen_expr_or_writefield(node, type_map, indent=0):
    """Generate Go code for an Argument node (can be Expression or WriteField)."""
    if node is None:
        return ""
    kind = node.get("kind")
    if kind == "WriteField":
        val = gen_expr(node.get("value"), type_map, indent)
        width = gen_expr(node.get("width"), type_map, indent) if node.get("width") else ""
        decimals = gen_expr(node.get("decimals"), type_map, indent) if node.get("decimals") else ""
        return f"{val}"  # Simplified: just the value
    return gen_expr(node, type_map, indent)


# ──────────────────────────────────────────────────────────────────
# Statement generation
# ──────────────────────────────────────────────────────────────────

def gen_stmt(stmt, type_map, labels, func_name="", indent=0):
    """Generate Go code from a Statement AST node."""
    if stmt is None:
        return ""
    kind = stmt.get("kind")
    prefix = "  " * indent
    suffix = "\n"

    if kind == "Empty":
        return f"{prefix}// empty\n"

    if kind == "Assignment":
        target = gen_expr(stmt.get("target"), type_map, indent)
        value = gen_expr(stmt.get("value"), type_map, indent)
        return f"{prefix}{target} = {value}\n"

    if kind == "Compound":
        body = stmt.get("statements", [])
        lines = [f"{prefix}{{\n"]
        for s in body:
            lines.append(gen_stmt(s, type_map, labels, func_name, indent + 1))
        lines.append(f"{prefix}}}\n")
        return "".join(lines)

    if kind == "If":
        cond = gen_expr(stmt.get("condition"), type_map, indent)
        then_body = gen_stmt(stmt.get("then"), type_map, labels, func_name, indent)
        else_body = gen_stmt(stmt.get("else"), type_map, labels, func_name, indent) if stmt.get("else") else ""
        result = f"{prefix}if {cond} {{\n{then_body}"
        if else_body:
            result += f"{prefix}}} else {{\n{else_body}"
        result += f"{prefix}}}\n"
        return result

    if kind == "While":
        cond = gen_expr(stmt.get("condition"), type_map, indent)
        body = gen_stmt(stmt.get("body"), type_map, labels, func_name, indent)
        return f"{prefix}for {cond} {{\n{body}{prefix}}}\n"

    if kind == "For":
        var = stmt["variable"]
        direction = stmt["direction"]
        initial = gen_expr(stmt.get("initial"), type_map, indent)
        final = gen_expr(stmt.get("final"), type_map, indent)
        body = gen_stmt(stmt.get("body"), type_map, labels, func_name, indent + 1)
        op = "<=" if direction == "to" else ">="
        step = "++" if direction == "to" else "--"
        return (f"{prefix}for {var} := {initial}; {var} {op} {final}; {var}{step} {{\n"
                f"{body}{prefix}}}\n")

    if kind == "Repeat":
        body = stmt.get("statements", [])
        body_code = ""
        for s in body:
            body_code += gen_stmt(s, type_map, labels, func_name, indent + 1)
        cond = gen_expr(stmt.get("condition"), type_map, indent + 1)
        return (f"{prefix}for {{\n{body_code}"
                f"{prefix}  if !({cond}) {{ break }}\n{prefix}}}\n")

    if kind == "Case":
        selector = gen_expr(stmt.get("selector"), type_map, indent)
        arms = stmt.get("arms", [])
        lines = [f"{prefix}switch {selector} {{\n"]
        for arm in arms:
            labels_list = arm.get("labels", [])
            body = gen_stmt(arm.get("statement"), type_map, labels, func_name, indent + 1)
            if labels_list == "others":
                lines.append(f"{prefix}default:\n{body}")
            else:
                for lbl in labels_list:
                    lbl_val = gen_expr(lbl, type_map, indent)
                    lines.append(f"{prefix}case {lbl_val}:\n{body}")
        lines.append(f"{prefix}}}\n")
        return "".join(lines)

    if kind == "Goto":
        label = stmt["label"]
        return f"{prefix}goto L{label}\n"

    if kind == "Labeled":
        label = stmt["label"]
        body = gen_stmt(stmt.get("statement"), type_map, labels, func_name, indent)
        return f"{prefix}L{label}:\n{body}"

    if kind == "ProcedureCall":
        name = stmt["name"]
        args = stmt.get("arguments", [])
        arg_strs = [gen_expr_or_writefield(a, type_map, indent) for a in args]
        return gen_proc_call(name, arg_strs, prefix)

    if kind == "With":
        records = stmt.get("records", [])
        body = gen_stmt(stmt.get("body"), type_map, labels, func_name, indent)
        return body

    return f"{prefix}// unhandled: {kind}\n"


# ──────────────────────────────────────────────────────────────────
# Declaration generation
# ──────────────────────────────────────────────────────────────────

def generate_record_go_def(record_type, name, type_map):
    """Generate a Go struct for a Pascal record type."""
    fixed = record_type.get("fixed", [])
    variant = record_type.get("variant")
    lines = [f"type {name}_t struct {{\n"]

    if variant:
        tag_type = pascal_type_to_go(variant["tag"]["type"], type_map)
        tag_name = variant["tag"].get("name")
        if tag_name:
            lines.append(f"\t{tag_name} {tag_type}\n")

    # Fixed fields
    for section in fixed:
        fields = section.get("fields", [])
        c_type = pascal_type_to_go(section.get("type"), type_map)
        for f in fields:
            lines.append(f"\t{f} {c_type}\n")

    # Variant parts (as embedded structs)
    if variant:
        for v in variant.get("variants", []):
            tag_labels = v.get("labels", [])
            if tag_labels:
                label_val = gen_expr(tag_labels[0], type_map)
            for section in v.get("fields", {}).get("fixed", []):
                fields = section.get("fields", [])
                c_type = pascal_type_to_go(section.get("type"), type_map)
                for f in fields:
                    lines.append(f"\t{f} {c_type}\n")

    lines.append("}\n\n")
    return "".join(lines)


def gen_type_decl(td, type_map):
    """Generate Go type alias from TypeDecl."""
    name = td["name"]
    ptype = td["type"]
    c_type = pascal_type_to_go(ptype, type_map)
    type_map[name] = f"*{name}_t" if name not in INT_TYPES and name not in CHAR_TYPES else c_type
    kind = ptype.get("kind")
    if kind == "RecordType":
        return generate_record_go_def(ptype, name, type_map)
    return f"type {name} = {c_type}\n"


def gen_var_decl(vd, type_map):
    """Generate Go variable declaration(s) from VarDecl."""
    names = vd.get("names", [])
    c_type = pascal_type_to_go(vd.get("type"), type_map)
    lines = []
    for name in names:
        lines.append(f"\t{name} {c_type}\n")
    return "".join(lines)


def gen_const_decl(cd):
    """Generate Go constant from ConstDecl."""
    name = cd["name"]
    value = gen_expr(cd.get("value"), {}, 0)
    return f"\t{name} = {value}\n"


def gen_param_decl(pd, type_map):
    """Generate Go parameter declaration from ParamDecl."""
    name = pd["name"]
    mode = pd.get("mode", "value")
    if pd.get("type"):
        c_type = pascal_type_to_go(pd["type"], type_map)
    else:
        c_type = "interface{}"  # procedure/function params
    if mode == "var":
        c_type = f"*{c_type}"  # var params → pointers in Go
    return f"{name} {c_type}"


# ──────────────────────────────────────────────────────────────────
# Subprogram generation
# ──────────────────────────────────────────────────────────────────

def _collect_label_usage(node, used, defined, depth=0):
    """Walk a statement tree collecting goto-target labels (used) and
    labels defined by Labeled nodes (defined)."""
    if depth > 40 or not isinstance(node, dict):
        return
    k = node.get("kind")
    if k == "Goto":
        used.add(node.get("label"))
    elif k == "Labeled":
        defined.add(node.get("label"))
    for v in node.values():
        if isinstance(v, (dict, list)):
            _collect_label_usage(v, used, defined, depth + 1)


def gen_subprogram(sp, type_map, all_labels):
    """Generate Go function from Subprogram.

    Go has no nested functions, so nested subprograms are NOT emitted here;
    they are hoisted to package level by a flattening collector in generate_go().
    Local types/consts/vars go INSIDE the function body (Go allows local type
    and const declarations)."""
    name = sp["name"]
    category = sp["category"]  # procedure or function
    forward = sp.get("forward", False)
    params = sp.get("parameters", [])
    return_type = sp.get("return_type")

    if not isinstance(all_labels, list):
        all_labels = list(all_labels)

    if forward:
        # Forward declaration stub
        param_strs = [gen_param_decl(p, type_map) for p in params]
        ret = pascal_type_to_go(return_type, type_map) if return_type else ""
        ret_str = f" {ret}" if ret else ""
        return f"func {go_safe_name(name)}({', '.join(param_strs)}){ret_str} {{ /* forward stub */ }}\n\n"

    if not sp.get("block"):
        param_strs = [gen_param_decl(p, type_map) for p in params]
        ret = pascal_type_to_go(return_type, type_map) if return_type else ""
        ret_str = f" {ret}" if ret else ""
        return f"func {go_safe_name(name)}({', '.join(param_strs)}){ret_str} {{ /* stub */ }}\n\n"

    block = sp["block"]
    local_labels = [lbl for lbl in block.get("labels", []) if isinstance(lbl, int)]

    # Function signature
    param_strs = [gen_param_decl(p, type_map) for p in params]
    ret = pascal_type_to_go(return_type, type_map) if return_type else ""
    ret_str = f" {ret}" if ret else ""

    lines = []
    lines.append(f"\n/* {category}: {name} */\n")
    lines.append(f"func {go_safe_name(name)}({', '.join(param_strs)}){ret_str} {{\n")

    # Local types are hoisted to package level by walk_and_emit_types in
    # generate_go(); Go allows local type decls too, but emitting them again
    # here would duplicate definitions. So we skip local types here.

    # Local consts (Go permits local const declarations)
    if block.get("constants"):
        lines.append("\tconst (\n")
        for cd in block.get("constants", []):
            lines.append(gen_const_decl(cd))
        lines.append("\t)\n")

    # Local variables
    if block.get("variables"):
        lines.append("\tvar (\n")
        for vd in block.get("variables", []):
            names = vd.get("names", [])
            c_type = pascal_type_to_go(vd.get("type"), type_map)
            for nm in names:
                lines.append(f"\t\t{nm} {c_type}\n")
        lines.append("\t)\n")

    # Body
    body = block.get("body", {})
    stmts = body.get("statements") or body.get("compound") or []
    for s in stmts:
        lines.append(gen_stmt(s, type_map, local_labels + all_labels, name, indent=1))

    # Goto labels: Go requires every used label to be defined, and forbids
    # defining a label that is never used. Emit definitions (at function end)
    # only for labels that are goto targets but not defined inline by a Labeled
    # node.
    used = set()
    defined = set()
    for s in stmts:
        _collect_label_usage(s, used, defined)
    for lbl in local_labels:
        if lbl in used and lbl not in defined:
            lines.append(f"\tL{lbl}:\n")

    lines.append("}\n\n")
    return "".join(lines)


# ──────────────────────────────────────────────────────────────────
# Main program body
# ──────────────────────────────────────────────────────────────────

def gen_main_body(block, type_map, all_labels):
    """Generate main() function body."""
    lines = []
    body = block.get("body", {})
    lines.append("\nfunc main() {\n")
    for s in body.get("statements", []):
        lines.append(gen_stmt(s, type_map, all_labels, "main"))
    lines.append("}\n")
    return "".join(lines)


# ──────────────────────────────────────────────────────────────────
# Header / preamble
# ──────────────────────────────────────────────────────────────────

def gen_header():
    """Go preamble: package and imports."""
    return """// CodeYAML2Go auto-generated Go source from TeX.yaml (Pascal-W AST)
// This is a transliteration of Knuth's TeX82, not a port.
// Many original TeX macros and WEB sections are stubbed out.

package main

import (
\t"os"
\t"fmt"
\t"math"
\t"time"
)

// ── Basic type aliases ──
type boolean = bool
const (
\ttrue  = true
\tfalse = false
)

// ── TeX constants (may be overridden in the code below) ──
const (
\tMAXPRINTLINE   = 79
\tMAXBUFSTACK    = 20
\tTEXTWIDTH      = 65
\tTEXTHEIGHT     = 53
\tDRAFTMODE      = false
\tMAXCOMPRESS    = 35
\tMAXSAVE        = 1000
\tMAXDEPTH       = 500
\tSTACKSIZE      = 200
\tMEMMAX         = 30000
\tBUFSIZE        = 500
\tMAXINOPEN      = 6
\tFONTMAX        = 75
\tFONTMEMSIZE    = 20000
\tPARAMSIZE      = 60
\tNESTSIZE       = 40
\tMAXSTRINGS     = 3000
\tPOOLSIZE       = 65535
\tMAXWIDTH       = 100000000
\tMAXHEIGHT      = 100000000
)

// ── I/O helpers ──
func print_(s string)     { fmt.Print(s) }
func println_()            { fmt.Println() }
func printc(c byte)      { fmt.Printf("%c", c) }
func newline()            { fmt.Println() }
func flush_out()          {}

// ── Pascal standard function stubs ──
func abs_(x int) int          { if x < 0 { return -x }; return x }
func sqr_(x int) int          { return x * x }
func round_(x float64) int    { return int(math.Floor(x + 0.5)) }
func trunc_(x float64) int    { return int(x) }
func eof_(args ...interface{}) bool { return false }
func eoln_(args ...interface{}) bool { return false }
func max_(args ...int) int {
	m := 0
	for _, v := range args { if v > m { m = v } }
	return m
}
func min_(args ...int) int {
	if len(args) == 0 { return 0 }
	m := args[0]
	for _, v := range args { if v < m { m = v } }
	return m
}
func copy_(s string, idx, n int) string {
	if idx < 1 { idx = 1 }
	if idx > len(s) { return "" }
	end := idx - 1 + n
	if end > len(s) { end = len(s) }
	return s[idx-1 : end]
}
func concat_(args ...string) string {
	var b []byte
	for _, s := range args { b = append(b, s...) }
	return string(b)
}
func pos_(sub, s string) int {
	for i := 0; i+len(sub) <= len(s); i++ {
		if s[i:i+len(sub)] == sub { return i + 1 }
	}
	return 0
}
func upcase_(c byte) byte {
	if c >= 'a' && c <= 'z' { return c - 32 }
	return c
}
func lo_(x int) int { return x & 0xFFFF }
func hi_(x int) int { return (x >> 16) & 0xFFFF }
func mem_(args ...interface{}) int { return 0 }

// ── Pascal standard procedure stubs (variadic to match any arity) ──
func get_(args ...interface{})     {}
func put_(args ...interface{})     {}
func reset_(args ...interface{})   {}
func rewrite_(args ...interface{}) {}
func read_(args ...interface{})    {}
func readln_(args ...interface{})  {}
func write_(args ...interface{})   {
	for _, a := range args { fmt.Print(a) }
}
func writeln_(args ...interface{}) {
	for _, a := range args { fmt.Print(a) }
	fmt.Println()
}
func page_(args ...interface{})    {}
func break_(args ...interface{})   {}
func pack_(args ...interface{})    {}
func unpack_(args ...interface{})  {}
func dispose_(args ...interface{}) {}
func new_(args ...interface{})     {}
func seek_(args ...interface{})    {}
func close_(args ...interface{})   {}
func flush_(args ...interface{})   {}

// ── Forward declarations (forward stubs) ──

"""


def gen_tail():
    """Go epilogue (the real main() is emitted by gen_main_body)."""
    return "// End of TeX.go\n"


# ──────────────────────────────────────────────────────────────────
# Collection helpers
# ──────────────────────────────────────────────────────────────────

def collect_forward_declarations(block):
    """Collect forward-declared subprograms (recursively, including nested)."""
    forwards = []

    def walk_block(b, depth=0):
        if depth > 12 or not isinstance(b, dict):
            return
        for s in b.get("subprograms", []):
            if s.get("forward"):
                forwards.append(s)
        for s in b.get("subprograms", []):
            if s.get("block") and not s.get("forward"):
                walk_block(s["block"], depth + 1)

    walk_block(block)
    return forwards


def collect_all_subprograms(block):
    """Flatten the subprogram tree to package-level functions.

    Go has no nested functions, so every subprogram (top-level and nested)
    is hoisted to package level. Order is pre-order: a parent is emitted
    before its children. Returns a list of subprogram nodes (excluding
    forward-declared ones, which are stubbed separately)."""
    result = []

    def walk_block(b, depth=0):
        if depth > 12 or not isinstance(b, dict):
            return
        for s in b.get("subprograms", []):
            if s.get("forward"):
                continue
            result.append(s)
        for s in b.get("subprograms", []):
            if s.get("block") and not s.get("forward"):
                walk_block(s["block"], depth + 1)

    walk_block(block)
    return result


def collect_all_labels(block):
    """Collect all goto labels from Block nodes (not CaseArm labels)."""
    labels = set()

    def walk(node, depth=0):
        if depth > 30:
            return
        if isinstance(node, dict):
            if "labels" in node and isinstance(node["labels"], list):
                for lbl in node["labels"]:
                    if isinstance(lbl, int):
                        labels.add(lbl)
            for v in node.values():
                walk(v, depth + 1)
        elif isinstance(node, list):
            for item in node:
                walk(item, depth + 1)

    walk(block)
    return labels


def build_type_map(block):
    """Build a map of Pascal type names to Go type strings."""
    type_map = {}
    # Pre-populate primitives
    for p in INT_TYPES:
        type_map[p] = "int"
    for p in CHAR_TYPES:
        type_map[p] = "byte"
    type_map["real"] = "float64"
    type_map["boolean"] = "bool"
    type_map["string"] = "string"

    def walk_block_types(b):
        if not isinstance(b, dict):
            return
        for td in b.get("types", []):
            name = td["name"]
            c_type = pascal_type_to_go(td["type"], type_map)
            type_map[name] = f"*{name}_t" if td["type"].get("kind") == "RecordType" else c_type
        for s in b.get("subprograms", []):
            if s.get("block"):
                walk_block_types(s["block"])

    walk_block_types(block)

    def _ensure_type(t, tm):
        if not t or not isinstance(t, dict):
            return
        k = t.get("kind")
        if k == "NamedType":
            if t["name"] not in tm:
                tm[t["name"]] = f"*{t['name']}_t"
        elif k == "ArrayType":
            _ensure_type(t.get("element_type"), tm)
            for idx in t.get("index_types", []):
                _ensure_type(idx, tm)

    def walk_block_vars(b):
        if not isinstance(b, dict):
            return
        for vd in b.get("variables", []):
            _ensure_type(vd.get("type"), type_map)
        for s in b.get("subprograms", []):
            if s.get("block"):
                walk_block_vars(s["block"])

    walk_block_vars(block)
    return type_map


# ──────────────────────────────────────────────────────────────────
# Main generator
# ──────────────────────────────────────────────────────────────────

def generate_go(doc):
    """Generate complete Go source from the parsed AST document."""
    block = doc["block"]
    type_map = build_type_map(block)
    all_labels = collect_all_labels(block)
    forwards = collect_forward_declarations(block)

    lines = [gen_header()]

    # Type definitions
    lines.append("/* ── Type definitions ── */\n")
    for td in block.get("types", []):
        lines.append(generate_record_go_def(td["type"], td["name"], type_map) if td["type"].get("kind") == "RecordType" else gen_type_decl(td, type_map))

    # Subprogram-level types
    def walk_and_emit_types(b):
        if not isinstance(b, dict):
            return
        for td in b.get("types", []):
            lines.append(generate_record_go_def(td["type"], td["name"], type_map) if td["type"].get("kind") == "RecordType" else gen_type_decl(td, type_map))
        for s in b.get("subprograms", []):
            if s.get("block"):
                walk_and_emit_types(s["block"])

    walk_and_emit_types(block)

    lines.append("\n")

    # Constants
    lines.append("/* ── Constants ── */\n")
    lines.append("const (\n")
    for cd in block.get("constants", []):
        lines.append(gen_const_decl(cd))
    lines.append(")\n\n")

    # Global variables
    lines.append("/* ── Global variables ── */\n")
    lines.append("var (\n")
    for vd in block.get("variables", []):
        name = vd["names"][0]  # First name
        c_type = pascal_type_to_go(vd.get("type"), type_map)
        lines.append(f"\t{name} {c_type}\n")
    lines.append(")\n\n")

    # Forward declarations
    lines.append("/* ── Forward declarations ── */\n")
    for fwd in forwards:
        lines.append(gen_subprogram(fwd, type_map, all_labels))

    # Subprograms (flattened to package level — Go has no nested functions)
    lines.append("/* ── Subprograms ── */\n")
    for sp in collect_all_subprograms(block):
        lines.append(gen_subprogram(sp, type_map, all_labels))

    # Main function
    lines.append("/* ── Main program ── */\n")
    lines.append(gen_main_body(block, type_map, all_labels))

    # Epilogue
    lines.append(gen_tail())

    return "".join(lines)


# ──────────────────────────────────────────────────────────────────
# Entry point
# ──────────────────────────────────────────────────────────────────

def main():
    yaml_path = sys.argv[1] if len(sys.argv) > 1 else "TeX.yaml"
    output_path = sys.argv[2] if len(sys.argv) > 2 else "TeX.go"

    print(f"Loading {yaml_path}...", file=sys.stderr)
    with open(yaml_path, "r") as f:
        doc = yaml.safe_load(f)

    print("Generating Go source...", file=sys.stderr)
    go_source = generate_go(doc)

    print(f"Writing {output_path}...", file=sys.stderr)
    with open(output_path, "w") as f:
        f.write(go_source)

    print(f"Done. Generated {len(go_source)} bytes → {output_path}", file=sys.stderr)


if __name__ == "__main__":
    main()
