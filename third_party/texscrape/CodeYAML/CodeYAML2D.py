#!/usr/bin/env python3
"""
CodeYAML2D.py — Compile TeX.yaml (Pascal-W AST in YAML) into TeX.d

Transliterates the Concrete-AST-PascalW YAML representation into D source.
Mirrors CodeYAML2C/CodeYAML2Go structure, mapping Pascal constructs to D.
"""

import sys
import yaml

# ──────────────────────────────────────────────────────────────────
# Reserved identifiers & mappings
# ──────────────────────────────────────────────────────────────────

D_RESERVED = {
    "abstract","alias","align","asm","assert","auto","body","bool","break",
    "byte","case","cast","catch","class","const","continue","debug","default",
    "delegate","delete","deprecated","do","else","enum","export","extern","false",
    "final","finally","for","foreach","foreach_reverse","function","goto","if",
    "immutable","import","in","inout","int","interface","invariant","is","lazy",
    "macro","mixin","module","new","nothrow","null","out","override","package",
    "pragma","private","protected","public","pure","real","ref","return","scope",
    "shared","short","static","struct","super","switch","synchronized","template",
    "this","throw","true","try","typeid","typeof","ubyte","ucent","uint","ulong",
    "union","unittest","ushort","version","void","volatile","while","with",
}

STD_FUNCS = {
    "abs":   lambda a: f"abs({a[0]})" if a else "abs(0)",
    "sqr":   lambda a: f"({a[0]} * {a[0]})" if a else "0",
    "odd":   lambda a: f"(({a[0]} & 1) != 0)" if a else "false",
    "round": lambda a: f"cast(int)round({a[0]})" if a else "0",
    "trunc": lambda a: f"cast(int)trunc({a[0]})" if a else "0",
    "chr":   lambda a: f"cast(char)({a[0]})" if a else "'\0'",
    "ord":   lambda a: f"cast(int)({a[0]})" if a else "0",
    "succ":  lambda a: f"({a[0]} + 1)",
    "pred":  lambda a: f"({a[0]} - 1)",
    "eof":   lambda a: "eof_()" if not a else f"eof_({a[0]})",
    "eoln":  lambda a: "eoln_()" if not a else f"eoln_({a[0]})",
    "max":   lambda a: f"max_({', '.join(a)})",
    "min":   lambda a: f"min_({', '.join(a)})",
    "length":lambda a: f"cast(int)({a[0]}.length)" if a else "0",
    "copy":  lambda a: f"copy_({', '.join(a)})",
    "concat":lambda a: f"concat_({', '.join(a)})",
    "pos":   lambda a: f"pos_({', '.join(a)})",
    "upcase":lambda a: f"toupper({a[0]})" if a else "'\0'",
    "lo":    lambda a: f"lo_({a[0]})",
    "hi":    lambda a: f"hi_({a[0]})",
    "mem":   lambda a: f"mem_({', '.join(a)})",
}

STD_PROCS = {
    "get", "put", "reset", "rewrite", "read", "readln",
    "write", "writeln", "page", "break", "pack", "unpack",
    "dispose", "new", "seek", "close", "flush",
}

INT_TYPES = {"integer","scaled","nonnegativeinteger","smallnumber","quarterword",
             "halfword","twochoices","fourchoices","groupcode","internalfontnumber",
             "fontindex","dviindex","hyphpointer","glueord","poolpointer","strnumber"}
CHAR_TYPES = {"char","ASCIIcode","eightbits","packedASCIIcode"}
RECORD_TYPES = {"twohalves","fourquarters","memoryword","liststaterecord","instaterecord"}
FILE_TYPES = {"alphafile","bytefile","wordfile"}

# ──────────────────────────────────────────────────────────────────
# Helpers
# ──────────────────────────────────────────────────────────────────

def d_safe(name:str)->str:
    if name in D_RESERVED:
        return name + "_"
    return name

# Type mapping

def pascal_type_to_d(type_obj, type_map):
    if type_obj is None:
        return "void"
    kind = type_obj.get("kind")
    if kind == "NamedType":
        name = type_obj["name"]
        if name in INT_TYPES:
            return "int"
        if name in CHAR_TYPES:
            return "char"
        if name == "real":
            return "double"
        if name == "boolean":
            return "bool"
        if name == "string":
            return "string"
        return type_map.get(name, f"{name}_t")
    if kind == "SubrangeType":
        return "int"
    if kind == "ArrayType":
        elem = pascal_type_to_d(type_obj.get("element_type"), type_map)
        return f"{elem}[]"
    if kind == "RecordType":
        return f"{type_obj.get('_gen_name', 'record_t')}"
    if kind == "FileType":
        return "File"
    if kind == "PointerType":
        base = pascal_type_to_d(type_obj.get("base"), type_map)
        return f"{base}*"
    return "auto"

# Expressions

def gen_expr(expr, type_map):
    if expr is None:
        return ""
    kind = expr.get("kind")
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
            return "null"
        return f"/* literal {lt}: {val} */"
    if kind == "Identifier":
        return expr["name"]
    if kind == "BinaryOp":
        op = expr["operator"]
        map_op = {"=":"==", "<>":"!=", "and":"&&", "or":"||", "div":"/", "mod":"%"}
        dop = map_op.get(op, op)
        left = gen_expr(expr.get("left"), type_map)
        right = gen_expr(expr.get("right"), type_map)
        return f"({left} {dop} {right})"
    if kind == "UnaryOp":
        op = expr["operator"]
        operand = gen_expr(expr.get("operand"), type_map)
        if op == "not":
            return f"(!{operand})"
        if op == "@":
            return f"&({operand})"
        return f"({op}{operand})"
    if kind == "Call":
        base = expr.get("base")
        name = base.get("name") if isinstance(base, dict) else None
        args = [gen_expr_or_writefield(a, type_map) for a in expr.get("arguments", [])]
        if name in STD_FUNCS:
            return STD_FUNCS[name](args)
        target = gen_expr(base, type_map)
        return f"{target}({', '.join(args)})"
    if kind == "Index":
        base = gen_expr(expr.get("base"), type_map)
        idxs = [gen_expr(i, type_map) for i in expr.get("indices", [])]
        return f"{base}[{']['.join(idxs)}]"
    if kind == "FieldAccess":
        base = gen_expr(expr.get("base"), type_map)
        return f"{base}.{expr['field']}"
    if kind == "Deref":
        base = gen_expr(expr.get("base"), type_map)
        return f"*{base}"
    if kind == "WriteField":
        return gen_expr(expr.get("value"), type_map)
    return f"/* expr {kind} */"


def gen_expr_or_writefield(node, type_map):
    if node is None:
        return ""
    if node.get("kind") == "WriteField":
        return gen_expr(node.get("value"), type_map)
    return gen_expr(node, type_map)

# Statements

def gen_stmt(stmt, type_map, labels, indent=0):
    if stmt is None:
        return ""
    kind = stmt.get("kind")
    prefix = "    " * indent
    if kind == "Empty":
        return f"{prefix};\n"
    if kind == "Assignment":
        tgt = gen_expr(stmt.get("target"), type_map)
        val = gen_expr(stmt.get("value"), type_map)
        return f"{prefix}{tgt} = {val};\n"
    if kind == "Compound":
        lines = [f"{prefix}{{\n"]
        for s in stmt.get("statements", []):
            lines.append(gen_stmt(s, type_map, labels, indent+1))
        lines.append(f"{prefix}}}\n")
        return ''.join(lines)
    if kind == "If":
        cond = gen_expr(stmt.get("condition"), type_map)
        then = gen_stmt(stmt.get("then"), type_map, labels, indent+1)
        result = f"{prefix}if ({cond}) {{\n{then}{prefix}}}\n"
        if stmt.get("else"):
            els = gen_stmt(stmt.get("else"), type_map, labels, indent+1)
            result = (f"{prefix}if ({cond}) {{\n{then}{prefix}}} else {{\n"
                      f"{els}{prefix}}}\n")
        return result
    if kind == "While":
        cond = gen_expr(stmt.get("condition"), type_map)
        body = gen_stmt(stmt.get("body"), type_map, labels, indent+1)
        return f"{prefix}while ({cond}) {{\n{body}{prefix}}}\n"
    if kind == "For":
        var = stmt["variable"]
        init = gen_expr(stmt.get("initial"), type_map)
        final = gen_expr(stmt.get("final"), type_map)
        direction = stmt.get("direction")
        step = "++" if direction == "to" else "--"
        op = "<=" if direction == "to" else ">="
        body = gen_stmt(stmt.get("body"), type_map, labels, indent+1)
        lines = [f"{prefix}for ({var} = {init}; {var} {op} {final}; {var}{step}) {{\n",
                 f"{body}{prefix}}}\n"]
        return ''.join(lines)
    if kind == "Repeat":
        body = ''.join(gen_stmt(s, type_map, labels, indent+1) for s in stmt.get("statements", []))
        cond = gen_expr(stmt.get("condition"), type_map)
        return f"{prefix}do {{\n{body}{prefix}}} while (!({cond}));\n"
    if kind == "Case":
        selector = gen_expr(stmt.get("selector"), type_map)
        lines = [f"{prefix}switch ({selector}) {{\n"]
        for arm in stmt.get("arms", []):
            labels_list = arm.get("labels", [])
            body = gen_stmt(arm.get("statement"), type_map, labels, indent+1)
            if labels_list == "others":
                lines.append(f"{prefix}default:\n{body}")
            else:
                for lbl in labels_list:
                    lbl_val = gen_expr(lbl, type_map)
                    lines.append(f"{prefix}case {lbl_val}:\n{body}{prefix}    break;\n")
        lines.append(f"{prefix}}}\n")
        return ''.join(lines)
    if kind == "Goto":
        return f"{prefix}goto L{stmt['label']};\n"
    if kind == "Labeled":
        label = stmt["label"]
        body = gen_stmt(stmt.get("statement"), type_map, labels, indent)
        return f"{prefix}L{label}:\n{body}"
    if kind == "ProcedureCall":
        name = stmt["name"]
        args = [gen_expr_or_writefield(a, type_map) for a in stmt.get("arguments", [])]
        if name in STD_PROCS:
            return f"{prefix}{name}_({', '.join(args)});\n"
        return f"{prefix}{d_safe(name)}({', '.join(args)});\n"
    if kind == "With":
        body = gen_stmt(stmt.get("body"), type_map, labels, indent)
        return body
    return f"{prefix}// unhandled {kind}\n"

# Type declarations & records

def generate_record_def(record_type, name, type_map):
    fixed = record_type.get("fixed", [])
    variant = record_type.get("variant")
    lines = [f"struct {name}_t {{\n"]
    if variant:
        tag = variant.get("tag", {})
        tag_type = pascal_type_to_d(tag.get("type"), type_map)
        tag_name = tag.get("name", "tag")
        lines.append(f"    {tag_type} {tag_name};\n")
    for section in fixed:
        fields = section.get("fields", [])
        ftype = pascal_type_to_d(section.get("type"), type_map)
        for f in fields:
            lines.append(f"    {ftype} {f};\n")
    if variant:
        for v in variant.get("variants", []):
            for section in v.get("fields", {}).get("fixed", []):
                fields = section.get("fields", [])
                ftype = pascal_type_to_d(section.get("type"), type_map)
                for f in fields:
                    lines.append(f"    {ftype} {f};\n")
    lines.append("};\n")
    return ''.join(lines)


def gen_type_decl(td, type_map):
    name = td["name"]
    ptype = td["type"]
    kind = ptype.get("kind")
    if kind == "RecordType":
        type_map[name] = f"{name}_t"
        return generate_record_def(ptype, name, type_map)
    ctype = pascal_type_to_d(ptype, type_map)
    type_map[name] = ctype
    return f"alias {name} = {ctype};\n"


def gen_var_decl(vd, type_map):
    names = vd.get("names", [])
    dtype = pascal_type_to_d(vd.get("type"), type_map)
    return ''.join(f"{dtype} {n};\n" for n in names)


def gen_const_decl(cd, type_map):
    name = cd["name"]
    val = gen_expr(cd.get("value"), type_map)
    return f"enum {name} = {val};\n"


def gen_param_decl(pd, type_map):
    dtype = pascal_type_to_d(pd.get("type"), type_map) if pd.get("type") else "auto"
    if pd.get("mode") == "var":
        dtype = f"ref {dtype}"
    return f"{dtype} {pd['name']}"

# Blocks & subprograms

def collect_forward(block):
    forwards = []
    def walk(b):
        if isinstance(b, dict):
            for s in b.get("subprograms", []):
                if s.get("forward"):
                    forwards.append(s)
                if s.get("block"):
                    walk(s["block"])
        elif isinstance(b, list):
            for x in b:
                walk(x)
    walk(block)
    return forwards


def collect_labels(block):
    labels = set()
    def walk(node):
        if isinstance(node, dict):
            if isinstance(node.get("labels"), list):
                for lbl in node["labels"]:
                    if isinstance(lbl, int):
                        labels.add(lbl)
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for x in node:
                walk(x)
    walk(block)
    return labels


def build_type_map(block):
    type_map = {}
    for name in INT_TYPES:
        type_map[name] = "int"
    for name in CHAR_TYPES:
        type_map[name] = "char"
    type_map["real"] = "double"
    type_map["boolean"] = "bool"
    def walk(node):
        if isinstance(node, dict):
            if node.get("kind") == "TypeDecl":
                nm = node["name"]
                ctype = pascal_type_to_d(node["type"], type_map)
                if node["type"].get("kind") == "RecordType":
                    type_map[nm] = f"{nm}_t"
                else:
                    type_map[nm] = ctype
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for x in node:
                walk(x)
    walk(block)
    return type_map


def gen_subprogram(sp, type_map, all_labels):
    name = d_safe(sp["name"])
    params = [gen_param_decl(p, type_map) for p in sp.get("parameters", [])]
    ret = pascal_type_to_d(sp.get("return_type"), type_map) if sp.get("return_type") else "void"
    if sp.get("forward"):
        return f"{ret} {name}({', '.join(params)});\n"
    block = sp.get("block")
    if not block:
        return f"{ret} {name}({', '.join(params)}) {{ /* forward stub */ }}\n"
    lines = [f"{ret} {name}({', '.join(params)})\n{{\n"]
    # Local decls
    for td in block.get("types", []):
        lines.append(gen_type_decl(td, type_map))
    for cd in block.get("constants", []):
        lines.append(gen_const_decl(cd, type_map))
    for vd in block.get("variables", []):
        lines.append(gen_var_decl(vd, type_map))
    for nested in block.get("subprograms", []):
        lines.append(gen_subprogram(nested, type_map, all_labels))
    for stmt in block.get("body", {}).get("statements", []):
        lines.append(gen_stmt(stmt, type_map, all_labels, 1))
    lines.append("}\n")
    return ''.join(lines)


def gen_main(block, type_map, all_labels):
    lines = ["int main(string[] args)\n{\n"]
    for stmt in block.get("body", {}).get("statements", []):
        lines.append(gen_stmt(stmt, type_map, all_labels, 1))
    lines.append("    return 0;\n}\n")
    return ''.join(lines)

# Header / tail

def gen_header():
    return """// Auto-generated D transliteration of TeX.yaml\nimport std.stdio;\nimport std.algorithm;\nimport std.array;\nimport std.string;\n"""


def gen_tail():
    return "\n// TODO: stub implementations for Pascal runtime hooks\n" 

# Main

def main():
    yaml_path = sys.argv[1] if len(sys.argv) > 1 else "TeX.yaml"
    out_path = sys.argv[2] if len(sys.argv) > 2 else "TeX.d"
    with open(yaml_path) as f:
        doc = yaml.safe_load(f)
    block = doc["block"]
    type_map = build_type_map(block)
    labels = collect_labels(block)
    lines = [gen_header()]
    for td in block.get("types", []):
        lines.append(gen_type_decl(td, type_map))
    lines.append("\n")
    for cd in block.get("constants", []):
        lines.append(gen_const_decl(cd, type_map))
    for vd in block.get("variables", []):
        lines.append(gen_var_decl(vd, type_map))
    for fwd in collect_forward(block):
        lines.append(gen_subprogram(fwd, type_map, labels))
    for sp in block.get("subprograms", []):
        if not sp.get("forward"):
            lines.append(gen_subprogram(sp, type_map, labels))
    lines.append(gen_main(block, type_map, labels))
    lines.append(gen_tail())
    out = ''.join(lines)
    with open(out_path, 'w') as f:
        f.write(out)

if __name__ == '__main__':
    main()
