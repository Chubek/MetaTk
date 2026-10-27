#!/usr/bin/env python3
"""
CodeYAML2C.py — Compile TeX.yaml (Pascal-W AST in YAML) into TeX.c

Produces compilable C11 from the Concrete-AST-PascalW YAML representation.
Handles WEB macros as C preprocessor stubs, Pascal types as C typedefs,
goto labels as switch/case jumps, var-params as pointers, etc.
"""

import json
import re
import sys
import yaml
from collections import OrderedDict

# ──────────────────────────────────────────────────────────────────
# Schema validation
# ──────────────────────────────────────────────────────────────────

SCHEMA_PATH = "CodeYAML.schema.json"

def validate_schema(doc):
    """Minimal validation: check root kind, block presence, node kinds."""
    if doc.get("kind") != "Program":
        raise ValueError(f"Expected root kind 'Program', got {doc.get('kind')}")
    if "block" not in doc:
        raise ValueError("Missing 'block' in Program root")
    return True

# ──────────────────────────────────────────────────────────────────
# Type mapping: Pascal → C
# ──────────────────────────────────────────────────────────────────

PRIMITIVE_TYPES = {"integer", "real", "boolean", "char", "string"}
FUNCTION_NAMES = set()
SUBPROGRAM_PARAMS = {}
CURRENT_FUNCTION = ""
CURRENT_SCOPE_TYPES = {}
GLOBAL_SCOPE_TYPES = {}

TEXT_FILE_FUNCTIONS = {
    "aopenin", "aopenout", "aclose", "inputln", "amakenamestring"
}
BYTE_FILE_FUNCTIONS = {
    "bopenin", "bopenout", "bclose", "bmakenamestring"
}
WORD_FILE_FUNCTIONS = {
    "wopenin", "wopenout", "wclose", "wmakenamestring"
}
FILE_RUNTIME_CALLS = {
    "reset", "rewrite", "erstat", "close", "eof", "eoln", "get", "put",
    "readln", "breakin",
}


def file_type_of_expression(expr):
    """Return alphafile/bytefile/wordfile for a known file expression."""
    if not isinstance(expr, dict):
        return None
    kind = expr.get("kind")
    if kind == "Identifier":
        name = expr["name"]
        if name == "f":
            if CURRENT_FUNCTION in TEXT_FILE_FUNCTIONS:
                return "alphafile"
            if CURRENT_FUNCTION in BYTE_FILE_FUNCTIONS:
                return "bytefile"
            if CURRENT_FUNCTION in WORD_FILE_FUNCTIONS:
                return "wordfile"
        return CURRENT_SCOPE_TYPES.get(name)
    if kind == "Index":
        base = expr.get("base", {})
        if base.get("kind") == "Identifier":
            return CURRENT_SCOPE_TYPES.get(base["name"])
    return None


def file_argument(expr, type_map):
    """Render a file variable as a pointer expected by the runtime."""
    rendered = gen_expr(expr, type_map)
    if (isinstance(expr, dict) and expr.get("kind") == "Identifier"
            and expr["name"] == "f"
            and CURRENT_FUNCTION in (
                TEXT_FILE_FUNCTIONS | BYTE_FILE_FUNCTIONS
                | WORD_FILE_FUNCTIONS)):
        return rendered
    return f"&({rendered})"


def gen_call_arguments(name, arguments, type_map):
    """Render arguments, taking addresses for Pascal var parameters."""
    parameters = SUBPROGRAM_PARAMS.get(name, [])
    rendered = []
    for index, argument in enumerate(arguments):
        value = gen_expr_or_writefield(argument, type_map)
        if index == 0 and name in FILE_RUNTIME_CALLS:
            value = file_argument(argument, type_map)
        elif index < len(parameters) and parameters[index].get("mode") == "var":
            value = f"&({value})"
        rendered.append(value)
    return rendered

def resolve_type_name(type_obj, type_map):
    """Resolve a Type AST node to a C type string."""
    if type_obj is None:
        return "void"
    kind = type_obj.get("kind")
    if kind == "NamedType":
        name = type_obj["name"]
        if name in ("integer", "scaled", "nonnegativeinteger", "smallnumber",
                     "glueratio", "quarterword", "halfword", "twochoices",
                     "fourchoices", "groupcode", "internalfontnumber",
                     "fontindex", "dviindex", "hyphpointer", "glueord"):
            return "int"
        if name == "real":
            return "double"
        if name == "boolean":
            return "int"  # C has no bool in C11 without stdbool.h; using int
        if name == "char":
            return "char"
        if name == "eightbits":
            return "unsigned char"
        if name in ("ASCIIcode", "packedASCIIcode"):
            return "unsigned char"
        if name == "poolpointer":
            return "int"
        if name == "strnumber":
            return "int"
        if name in ("twohalves", "fourquarters", "memoryword",
                     "liststaterecord", "instaterecord"):
            return f"struct {name}_t"
        if name in type_map:
            return type_map[name]
        return f"/* {name} */ int"
    if kind == "SubrangeType":
        return "int"
    if kind == "ArrayType":
        elem = resolve_type_name(type_obj.get("element_type"), type_map)
        parts = []
        for idx_type in type_obj.get("index_types", []):
            parts.append(resolve_type_name(idx_type, type_map))
        return f"{elem} *"
    if kind == "RecordType":
        return "void *"  # Opaque handle; Pascal records map to C structs
    if kind == "FileType":
        elem = type_obj.get("element_type", {})
        if elem.get("kind") == "NamedType" and elem["name"] == "char":
            return "FILE *"
        return "FILE *"
    if kind == "PointerType":
        base = resolve_type_name(type_obj.get("base"), type_map)
        return f"{base} *"
    return "/* unknown type: {kind} */ int"


def pascal_type_to_c(type_obj, type_map):
    """Convert Pascal Type to C type, handling common patterns."""
    if type_obj is None:
        return "void"
    kind = type_obj.get("kind")

    if kind == "NamedType":
        name = type_obj["name"]
        # Integer-like types
        if name in ("integer", "scaled", "nonnegativeinteger", "smallnumber",
                     "twochoices", "fourchoices",
                     "groupcode", "internalfontnumber", "fontindex",
                     "dviindex", "hyphpointer", "glueord", "poolpointer",
                     "strnumber"):
            return "int"
        if name in ("ASCIIcode", "eightbits", "packedASCIIcode",
                    "quarterword"):
            return "uint8_t"
        if name == "halfword":
            return "uint16_t"
        if name == "real":
            return "double"
        if name == "boolean":
            return "int"
        if name == "char":
            return "char"
        # Record types
        if name in ("twohalves", "fourquarters", "memoryword"):
            return f"{name}_t"
        if name == "liststaterecord":
            return "liststaterecord_t"
        if name == "instaterecord":
            return "instaterecord_t"
        if name in type_map:
            return type_map[name]
        return f"int /* {name} */"

    if kind == "SubrangeType":
        return "int"  # Subranges are integer-based

    if kind == "ArrayType":
        elem = pascal_type_to_c(type_obj.get("element_type"), type_map)
        return f"{elem} *"  # Pointers for arrays

    if kind == "RecordType":
        return "void *"  # Opaque pointer

    if kind == "FileType":
        return "FILE *"

    if kind == "PointerType":
        base = pascal_type_to_c(type_obj.get("base"), type_map)
        return f"{base} *"

    return "/* unknown */ int"


# ──────────────────────────────────────────────────────────────────
# Expression generation
# ──────────────────────────────────────────────────────────────────

def gen_expr(expr, type_map):
    """Generate C expression code from an Expression AST node."""
    if expr is None:
        return "/* NULL expr */"
    kind = expr.get("kind")

    if kind == "Literal":
        lt = expr.get("literal_type")
        val = expr.get("value")
        if lt == "integer":
            return str(val)
        if lt == "real":
            return f"{val:.10g}"
        if lt == "string":
            s = str(val)
            if len(s) == 1:
                escaped = (s.replace("\\", "\\\\")
                            .replace("'", "\\'")
                            .replace("\n", "\\n")
                            .replace("\r", "\\r")
                            .replace("\t", "\\t"))
                return f"'{escaped}'"
            # Pascal string literal → C string
            # Escape for C
            s = s.replace("\\", "\\\\")
            s = s.replace('"', '\\"')
            return f'"{s}"'
        if lt == "boolean":
            return "1" if val else "0"
        if lt == "nil":
            return "NULL"
        return f"/* literal {lt}: {val} */"

    if kind == "Identifier":
        name = expr["name"]
        return f"{name}()" if name in FUNCTION_NAMES else name

    if kind == "BinaryOp":
        op = expr["operator"]
        left = gen_expr(expr.get("left"), type_map)
        right = gen_expr(expr.get("right"), type_map)
        # Map Pascal operators to C
        map_op = {"=": "==", "<>": "!=", "and": "&&", "or": "||",
                   "div": "/", "mod": "%"}
        c_op = map_op.get(op, op)
        return f"({left} {c_op} {right})"

    if kind == "UnaryOp":
        op = expr["operator"]
        if op == "not":
            op = "!"
        operand = gen_expr(expr.get("operand"), type_map)
        return f"({op}{operand})"

    if kind == "Call":
        base_node = expr.get("base")
        if base_node and base_node.get("kind") == "Identifier":
            base = base_node["name"]
        else:
            base = gen_expr(base_node, type_map)
        args = expr.get("arguments", [])
        arg_strs = gen_call_arguments(base, args, type_map)
        if (base == "abs" and CURRENT_FUNCTION == "shownodelist"
                and len(arg_strs) == 1 and arg_strs[0] == "g"):
            base = "fabs"
        return f"{base}({', '.join(arg_strs)})"

    if kind == "Index":
        base = gen_expr(expr.get("base"), type_map)
        indices = expr.get("indices", [])
        idx_strs = []
        for idx in indices:
            rendered = gen_expr(idx, type_map)
            if base == "trieophash":
                rendered = f"({rendered} + trieopsize)"
            elif base == "xord":
                rendered = f"(unsigned char)({rendered})"
            idx_strs.append(rendered)
        return f"{base}[{', '.join(idx_strs)}]"

    if kind == "FieldAccess":
        base = gen_expr(expr.get("base"), type_map)
        field = expr["field"]
        if field == "int":
            field = "cint"
        return f"{base}.{field}"

    if kind == "Deref":
        base_node = expr.get("base")
        base = gen_expr(base_node, type_map)
        file_type = file_type_of_expression(base_node)
        if file_type:
            if (base_node.get("kind") == "Identifier"
                    and base_node["name"] == "f"
                    and CURRENT_FUNCTION in (
                        TEXT_FILE_FUNCTIONS | BYTE_FILE_FUNCTIONS
                        | WORD_FILE_FUNCTIONS)):
                return f"{base}->current"
            return f"{base}.current"
        return f"(*{base})"

    if kind == "WriteField":
        # In expression context, just emit the value
        return gen_expr(expr.get("value"), type_map)

    return f"/* unknown expr kind: {kind} */"


def gen_expr_or_writefield(node, type_map):
    """Generate C code for an Argument node (can be Expression or WriteField)."""
    if node is None:
        return ""
    kind = node.get("kind")
    if kind == "WriteField":
        # write(w, x:5) → fprintf(w, "%5g", x)
        # For simplicity in call args, just emit value with format hint
        val = gen_expr(node.get("value"), type_map)
        width = gen_expr(node.get("width"), type_map) if node.get("width") else ""
        decimals = gen_expr(node.get("decimals"), type_map) if node.get("decimals") else ""
        # Return a printf format pair for write/writeln
        return f"{{.val={val}, .w={width}, .d={decimals}}}"
    return gen_expr(node, type_map)


# ──────────────────────────────────────────────────────────────────
# Statement generation
# ──────────────────────────────────────────────────────────────────

def gen_stmt(stmt, type_map, labels, func_name="", indent=0):
    """Generate C code from a Statement AST node."""
    if stmt is None:
        return ""
    kind = stmt.get("kind")
    prefix = "  " * indent
    suffix = "\n"

    if kind == "Empty":
        return f"{prefix};\n"

    if kind == "Assignment":
        target_node = stmt.get("target")
        if target_node and target_node.get("kind") == "Identifier":
            target = target_node["name"]
        else:
            target = gen_expr(target_node, type_map)
        value = gen_expr(stmt.get("value"), type_map)
        if target in ("nameoffile", "TEXformatdefault", "months"):
            return f"{prefix}strcpy({target} + 1, {value});\n"
        return f"{prefix}{target} = {value};\n"

    if kind == "Compound":
        body = stmt.get("statements", [])
        lines = [f"{prefix}{{\n"]
        for s in body:
            lines.append(gen_stmt(s, type_map, labels, func_name, indent + 1))
        lines.append(f"{prefix}}}\n")
        return "".join(lines)

    if kind == "If":
        cond = gen_expr(stmt.get("condition"), type_map)
        then_body = gen_stmt(
            stmt.get("then"), type_map, labels, func_name, indent + 1)
        else_body = gen_stmt(
            stmt.get("else"), type_map, labels, func_name, indent + 1
        ) if stmt.get("else") else ""
        result = f"{prefix}if ({cond}) {{\n{then_body}{prefix}}}"
        if else_body:
            result += f" else {{\n{else_body}{prefix}}}"
        return result + "\n"

    if kind == "While":
        cond = gen_expr(stmt.get("condition"), type_map)
        body = gen_stmt(
            stmt.get("body"), type_map, labels, func_name, indent + 1)
        return f"{prefix}while ({cond}) {{\n{body}{prefix}}}\n"

    if kind == "For":
        var = stmt["variable"]
        direction = stmt["direction"]
        initial = gen_expr(stmt.get("initial"), type_map)
        final = gen_expr(stmt.get("final"), type_map)
        body = gen_stmt(
            stmt.get("body"), type_map, labels, func_name, indent + 1)
        if direction == "to":
            return (f"{prefix}for ({var} = {initial}; {var} <= {final}; "
                    f"++{var}) {{\n{body}{prefix}}}\n")
        else:  # downto
            return (f"{prefix}for ({var} = {initial}; {var} >= {final}; "
                    f"--{var}) {{\n{body}{prefix}}}\n")

    if kind == "Repeat":
        body = stmt.get("statements", [])
        body_code = ""
        for s in body:
            body_code += gen_stmt(s, type_map, labels, func_name, indent + 1)
        cond = gen_expr(stmt.get("condition"), type_map)
        return f"{prefix}do {{\n{body_code}{prefix}}} while (!({cond}));\n"

    if kind == "Case":
        selector = gen_expr(stmt.get("selector"), type_map)
        arms = stmt.get("arms", [])
        lines = [f"{prefix}switch ({selector}) {{\n"]
        for arm in arms:
            labels_list = arm.get("labels", [])
            body = gen_stmt(arm.get("statement"), type_map, labels, func_name, indent + 1)
            if labels_list == "others":
                lines.append(f"{prefix}default:\n{body}{prefix}  break;\n")
            else:
                for lbl in labels_list:
                    lbl_val = gen_expr(lbl, type_map)
                    lines.append(f"{prefix}case {lbl_val}:\n")
                lines.append(body)
                lines.append(f"{prefix}  break;\n")
        lines.append(f"{prefix}}}\n")
        return "".join(lines)

    if kind == "Goto":
        label = stmt["label"]
        if label not in labels:
            return f"{prefix}tex_jump({label});\n"
        return f"{prefix}goto L{label};\n"

    if kind == "Labeled":
        label = stmt["label"]
        body = gen_stmt(stmt.get("statement"), type_map, labels, func_name, indent)
        return f"{prefix}L{label}:\n{body}"

    if kind == "ProcedureCall":
        name = stmt["name"]
        args = stmt.get("arguments", [])
        if name in ("write", "writeln"):
            if not args:
                return f"{prefix}/* malformed Pascal {name} */\n"
            file_node = args[0]
            file_type = file_type_of_expression(file_node)
            file_ptr = file_argument(file_node, type_map)
            lines = []
            for argument in args[1:]:
                if file_type == "bytefile":
                    value = gen_expr(argument, type_map)
                    lines.append(
                        f"{prefix}tex_byte_write({file_ptr}, {value});\n")
                elif argument.get("kind") == "WriteField":
                    value = gen_expr(argument.get("value"), type_map)
                    width = gen_expr(argument.get("width"), type_map) \
                        if argument.get("width") else "0"
                    lines.append(
                        f"{prefix}tex_text_write_int({file_ptr}, "
                        f"{value}, {width});\n")
                elif (argument.get("kind") == "Literal"
                      and argument.get("literal_type") == "string"
                      and len(str(argument.get("value"))) != 1):
                    value = gen_expr(argument, type_map)
                    lines.append(
                        f"{prefix}tex_text_write_string({file_ptr}, "
                        f"{value});\n")
                else:
                    value = gen_expr(argument, type_map)
                    lines.append(
                        f"{prefix}tex_text_write_char({file_ptr}, {value});\n")
            if name == "writeln":
                lines.append(f"{prefix}tex_text_newline({file_ptr});\n")
            return "".join(lines)
        if name == "read":
            file_node = args[0]
            file_ptr = file_argument(file_node, type_map)
            lines = []
            for argument in args[1:]:
                target = gen_expr(argument, type_map)
                lines.append(
                    f"{prefix}{target} = ({file_ptr})->current;\n")
                lines.append(f"{prefix}tex_text_get({file_ptr});\n")
            return "".join(lines)
        if name == "readln":
            return (f"{prefix}tex_text_readln("
                    f"{file_argument(args[0], type_map)});\n")
        if name == "break":
            name = "pascal_break"
        arg_strs = gen_call_arguments(stmt["name"], args, type_map)
        return f"{prefix}{name}({', '.join(arg_strs)});\n"

    if kind == "With":
        records = stmt.get("records", [])
        recs = [gen_expr(r, type_map) for r in records]
        body = gen_stmt(stmt.get("body"), type_map, labels, func_name, indent)
        # Pascal 'with' → nested access; just emit the body
        return body

    return f"{prefix}/* unhandled: {kind} */\n"


# ──────────────────────────────────────────────────────────────────
# Designator generation (lvalue)
# ──────────────────────────────────────────────────────────────────

def gen_designator(node, type_map):
    """Generate C lvalue code from a Designator (Expression subtype)."""
    return gen_expr(node, type_map)


# ──────────────────────────────────────────────────────────────────
# Declaration generation
# ──────────────────────────────────────────────────────────────────

def gen_type_decl(td, type_map):
    """Generate C typedef from TypeDecl."""
    name = td["name"]
    ptype = td["type"]
    c_type = pascal_type_to_c(ptype, type_map)
    type_map[name] = c_type
    if name == "alphafile":
        type_map[name] = "alphafile"
        return "typedef tex_text_file_t alphafile;\n"
    if name == "bytefile":
        type_map[name] = "bytefile"
        return "typedef tex_byte_file_t bytefile;\n"
    if name == "wordfile":
        type_map[name] = "wordfile"
        return """typedef struct wordfile {
    FILE *handle;
    memoryword_t current;
    int status;
    bool at_eof;
} wordfile;
"""
    # Generate struct/union definitions for records
    kind = ptype.get("kind")
    if kind == "RecordType":
        return generate_record_cdef(ptype, name, type_map)
    return f"typedef {c_type} {name};\n"


def generate_record_cdef(record_type, name, type_map):
    """Generate a C struct/union for a Pascal record type."""
    if name == "twohalves":
        return """typedef struct twohalves_t {
    halfword rh;
    union {
        halfword lh;
        struct {
            quarterword b0;
            quarterword b1;
        };
    };
} twohalves_t;
"""
    if name == "fourquarters":
        return """typedef struct fourquarters_t {
    quarterword b0;
    quarterword b1;
    quarterword b2;
    quarterword b3;
} fourquarters_t;
"""
    if name == "memoryword":
        return """typedef union memoryword_t {
    int cint;
    double gr;
    twohalves_t hh;
    fourquarters_t qqqq;
} memoryword_t;
"""

    fixed = record_type.get("fixed", [])
    variant = record_type.get("variant")
    lines = [f"typedef struct {name}_t {{\n"]

    if variant:
        tag_type = pascal_type_to_c(variant["tag"]["type"], type_map)
        tag_name = variant["tag"].get("name")
        if tag_name:
            lines.append(f"    {tag_type} {tag_name};\n")

    # Fixed fields
    for section in fixed:
        fields = section.get("fields", [])
        c_type = pascal_type_to_c(section.get("type"), type_map)
        for f in fields:
            lines.append(f"    {c_type} {f};\n")

    # Variant parts
    if variant:
        for v in variant.get("variants", []):
            for section in v.get("fields", {}).get("fixed", []):
                fields = section.get("fields", [])
                c_type = pascal_type_to_c(section.get("type"), type_map)
                for f in fields:
                    lines.append(f"    {c_type} {f};\n")

    lines.append(f"}} {name}_t;\n")
    return "".join(lines)


def gen_var_decl(vd, type_map):
    """Generate C variable declaration(s) from VarDecl."""
    names = vd.get("names", [])
    pascal_type = vd.get("type")
    c_type = pascal_type_to_c(pascal_type, type_map)
    prefix = "static "
    lines = []
    for name in names:
        lines.append(gen_one_var(name, pascal_type, type_map, prefix))
    return "".join(lines)


def gen_one_var(name, pascal_type, type_map, prefix=""):
    """Generate one scalar or fixed Pascal array declaration."""
    if pascal_type and pascal_type.get("kind") == "ArrayType":
        element = pascal_type_to_c(pascal_type.get("element_type"), type_map)
        dimensions = []
        named_upper_bounds = {
            "char": "255",
            "ASCIIcode": "255",
            "eightbits": "255",
            "packedASCIIcode": "255",
            "poolpointer": "poolsize",
            "strnumber": "maxstrings",
            "internalfontnumber": "fontmax",
            "fontindex": "fontmemsize",
            "dviindex": "dvibufsize",
            "triepointer": "triesize",
            "hyphpointer": "307",
            "glueord": "3",
        }
        for index_type in pascal_type.get("index_types", []):
            if index_type.get("kind") == "NamedType":
                upper = named_upper_bounds[index_type["name"]]
            else:
                upper = gen_expr(index_type.get("upper"), type_map)
            if name == "trieophash":
                dimensions.append(f"[(2 * ({upper})) + 1]")
            else:
                # Preserve Pascal's direct indices by retaining unused slots
                # below a nonnegative lower bound. The extra slot also holds
                # a C terminator when a packed char array receives a string.
                dimensions.append(f"[({upper}) + 2]")
        return (f"{prefix}{element} {name}{''.join(dimensions)}"
                " = {0};\n")
    return (f"{prefix}{pascal_type_to_c(pascal_type, type_map)} {name}"
            " = {0};\n")


def gen_const_decl(cd, type_map):
    """Generate C #define from ConstDecl."""
    name = cd["name"]
    value = gen_expr(cd.get("value"), type_map)
    return f"#define {name} ({value})\n"


def gen_param_decl(pd, type_map):
    """Generate C parameter declaration from ParamDecl."""
    name = pd["name"]
    mode = pd.get("mode", "value")
    if pd.get("type"):
        c_type = pascal_type_to_c(pd["type"], type_map)
    else:
        c_type = "void *"  # procedure/function params
    if mode == "var":
        c_type = f"{c_type} *"  # var params → pointers
    return f"{c_type} {name}"


# ──────────────────────────────────────────────────────────────────
# Subprogram generation
# ──────────────────────────────────────────────────────────────────

def gen_subprogram(sp, type_map, all_labels):
    """Generate C function/procedure from Subprogram."""
    global CURRENT_FUNCTION, CURRENT_SCOPE_TYPES
    name = sp["name"]
    category = sp["category"]  # procedure or function
    forward = sp.get("forward", False)
    params = sp.get("parameters", [])
    return_type = sp.get("return_type")

    if forward:
        # Forward declaration
        param_strs = []
        for p in params:
            param_strs.append(gen_param_decl(p, type_map))
        ret = pascal_type_to_c(return_type, type_map) if return_type else "void"
        return f"{ret} {name}({', '.join(param_strs)});\n"

    if not sp.get("block"):
        # Forward stub
        param_strs = []
        for p in params:
            param_strs.append(gen_param_decl(p, type_map))
        ret = pascal_type_to_c(return_type, type_map) if return_type else "void"
        return f"{ret} {name}({', '.join(param_strs)}) {{ /* forward stub */ }}\n"

    block = sp["block"]
    previous_function = CURRENT_FUNCTION
    previous_scope = CURRENT_SCOPE_TYPES
    CURRENT_FUNCTION = name
    CURRENT_SCOPE_TYPES = dict(GLOBAL_SCOPE_TYPES)
    for parameter in params:
        parameter_type = parameter.get("type", {})
        if parameter_type.get("kind") == "NamedType":
            CURRENT_SCOPE_TYPES[parameter["name"]] = parameter_type["name"]
    for variable in block.get("variables", []):
        variable_type = variable.get("type", {})
        if variable_type.get("kind") == "NamedType":
            type_name = variable_type["name"]
        elif (variable_type.get("kind") == "ArrayType"
              and variable_type.get("element_type", {}).get("kind")
              == "NamedType"):
            type_name = variable_type["element_type"]["name"]
        else:
            type_name = None
        for variable_name in variable.get("names", []):
            CURRENT_SCOPE_TYPES[variable_name] = type_name
    # Function signature
    lines = []

    # Local types
    for td in block.get("types", []):
        lines.append(gen_type_decl(td, type_map))

    # Local consts as #defines
    for cd in block.get("constants", []):
        lines.append(gen_const_decl(cd, type_map))

    # Collect local labels
    local_labels = block.get("labels", [])

    # Function signature - ensure all_labels is a list
    if not isinstance(all_labels, list):
        all_labels = list(all_labels)
    param_strs = []
    for p in params:
        param_strs.append(gen_param_decl(p, type_map))
    ret = pascal_type_to_c(return_type, type_map) if return_type else "void"

    lines.append(f"\n/* {category}: {name} */\n")
    lines.append(f"{ret} {name}({', '.join(param_strs)})\n")
    lines.append("{\n")

    # Pascal locals have automatic storage duration.
    for vd in block.get("variables", []):
        for variable_name in vd.get("names", []):
            lines.append(gen_one_var(
                variable_name, vd.get("type"), type_map, "  "))
    local_names = {
        variable_name
        for vd in block.get("variables", [])
        for variable_name in vd.get("names", [])
    }
    if return_type and "result" not in local_names:
        lines.append(f"  {ret} result = ({ret})0;\n")

    # Body
    body = block.get("body", {})
    if "statements" in body:
        for s in body["statements"]:
            statement = gen_stmt(
                s, type_map, local_labels, name, indent=1)
            if return_type:
                statement = re.sub(
                    rf"(?m)^(\s*){re.escape(name)}\s*=",
                    r"\1result =", statement)
            lines.append(statement)
    elif "compound" in body:
        for s in body["compound"]:
            statement = gen_stmt(
                s, type_map, local_labels, name, indent=1)
            if return_type:
                statement = re.sub(
                    rf"(?m)^(\s*){re.escape(name)}\s*=",
                    r"\1result =", statement)
            lines.append(statement)

    # Handle goto labels: Pascal goto jumps to labeled statements
    # Generate labels as C labels within the function
    for lbl in local_labels:
        # We need to find the Labeled statement for this label
        # Labels are defined in the block
        pass  # Labels are handled inline by Labeled nodes

    if name == "primitive":
        lines.append("""#ifdef TEX_DEBUG
  if (s == 535)
    fprintf(stderr, "DEBUG primitive relax: curval=%d range=%d..%d "
            "cmd=%u chr=%u hash=%u\\n", curval, strstart[s],
            strstart[s + 1], eqtb[curval].hh.b0, eqtb[curval].hh.rh,
            hash[curval].rh);
#endif
""")
    if name == "getnext":
        lines.append("""#ifdef TEX_DEBUG
  fprintf(stderr, "DEBUG token: cs=%u cmd=%u chr=%u hash=%u\\n",
          curcs, curcmd, curchr, curcs ? hash[curcs].rh : 0);
#endif
""")
    if name == "fixdateandtime":
        lines.append("""  {
    time_t now = time(NULL);
    struct tm local_now;
#if defined(_POSIX_VERSION)
    localtime_r(&now, &local_now);
#else
    local_now = *localtime(&now);
#endif
    systime = local_now.tm_hour * 60 + local_now.tm_min;
    sysday = local_now.tm_mday;
    sysmonth = local_now.tm_mon + 1;
    sysyear = local_now.tm_year + 1900;
  }
""")
    if name == "getstringsstarted":
        lines.append("  (void)g;\n")
    if name == "showwhatever":
        lines.append("  (void)p;\n")
    if return_type:
        lines.append("  return result;\n")
    lines.append("}\n\n")

    result_text = "".join(lines)
    CURRENT_FUNCTION = previous_function
    CURRENT_SCOPE_TYPES = previous_scope
    return result_text


# ──────────────────────────────────────────────────────────────────
# Main program body
# ──────────────────────────────────────────────────────────────────

def gen_main_body(block, type_map, all_labels):
    """Generate main() function body."""
    lines = []
    body = block.get("body", {})
    lines.append("\nint main(int argc, char *argv[])\n")
    lines.append("{\n")
    lines.append("  tex_runtime_set_arguments(argc, argv);\n")
    lines.append("  int tex_jump_label = setjmp(tex_exit_environment);\n")
    lines.append("  tex_exit_environment_ready = true;\n")
    lines.append("  if (tex_jump_label == 9998) goto L9998;\n")
    lines.append("  if (tex_jump_label == 9999) goto L9999;\n")

    # Ensure all_labels is a list
    if not isinstance(all_labels, list):
        all_labels = list(all_labels)
    for s in body.get("statements", []):
        lines.append(gen_stmt(s, type_map, all_labels, "main", indent=1))

    lines.append("    return history == 0 ? 0 : 1;\n")
    lines.append("}\n")
    return "".join(lines)


# ──────────────────────────────────────────────────────────────────
# Header / preamble
# ──────────────────────────────────────────────────────────────────

def gen_header():
    """C preamble: includes and basic type definitions."""
    return """/*
 * TeX.c — Auto-generated from TeX.yaml (Pascal-W AST)
 * Generated by CodeYAML2C.py
 *
 * Working native C port of Knuth's TeX82 Pascal-H program.
 *
 * Build:
 *   cc -std=gnu11 -O2 Working/TeX.c -lm -o texscrape
 *
 * The program starts in INITEX mode. It accepts TeX's initial command line
 * as normal argv entries and searches TEXINPUTS, TEXFONTS, and TEXFORMATS.
 *
 * Bootstrap plain TeX:
 *   TEXINPUTS=/path/to/plain:/path/to/hyphen \
 *   TEXFONTS=/path/to/cm:/path/to/knuth-lib \
 *     ./texscrape '\\input plain \\dump'
 *
 * Compile a document with that format:
 *   TEXFORMATS=. TEXFONTS=/path/to/cm ./texscrape '&plain' document.tex
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <setjmp.h>
#include <unistd.h>
#include <time.h>

/* Avoid collision with the C math library's remainder(3). */
#define remainder tex_remainder

/* ── Basic Pascal type aliases ── */
typedef bool boolean;

typedef struct tex_text_file {
    FILE *handle;
    int current;
    int status;
    bool at_eof;
    bool at_eoln;
    bool synthetic_eol;
} tex_text_file_t;

typedef struct tex_byte_file {
    FILE *handle;
    uint8_t current;
    int status;
    bool at_eof;
} tex_byte_file_t;

/* ── TeX constants (from Pascal consts, may be overridden) ── */
#ifndef MAXPRINTLINE
#define MAXPRINTLINE 79
#endif
#ifndef MAXBUFSTACK
#define MAXBUFSTACK 20
#endif
#ifndef TEXTWIDTH
#define TEXTWIDTH 65
#endif
#ifndef TEXTHEIGHT
#define TEXTHEIGHT 53
#endif
#ifndef DRAFTMODE
#define DRAFTMODE false
#endif
#ifndef MAXCOMPRESS
#define MAXCOMPRESS 35
#endif
#ifndef MAXSAVE
#define MAXSAVE 1000
#endif
#ifndef MAXDEPTH
#define MAXDEPTH 500
#endif
#ifndef STACKSIZE
#define STACKSIZE 200
#endif
#ifndef MEMMAX
#define MEMMAX 30000
#endif
#ifndef BUFSIZE
#define BUFSIZE 500
#endif
#ifndef MAXINOPEN
#define MAXINOPEN 6
#endif
#ifndef FONTMAX
#define FONTMAX 75
#endif
#ifndef FONTMEMSIZE
#define FONTMEMSIZE 20000
#endif
#ifndef PARAMSIZE
#define PARAMSIZE 60
#endif
#ifndef NESTSIZE
#define NESTSIZE 40
#endif
#ifndef MAXSTRINGS
#define MAXSTRINGS 3000
#endif
#ifndef POOLSIZE
#define POOLSIZE 65535
#endif
#ifndef MAXWIDTH
#define MAXWIDTH 100000000
#endif
#ifndef MAXHEIGHT
#define MAXHEIGHT 100000000
#endif

/* ── Debug/trace support ── */
#define trace(s)      /* disabled */
#define xtrace(s)     /* disabled */
#define show_it(...)  /* disabled */

/* ── String operations (Pascal-like) ── */
typedef struct {
    int length;
    char *data;
} string_t;

#define stralloc(n)     (char *)calloc((n)+1, 1)
#define strassign(dst, src)  do { if (dst) { free(dst); } dst = stralloc(strlen(src)); memcpy(dst, src, strlen(src)); } while(0)
#define stringeq(a, b)  (strcmp(a, b) == 0)

/* ── Character set ops ── */
#define chr(c)          ((char)(c))
#define ord(c)          ((int)(unsigned char)(c))
#define ordchr(x)       ((char)(x))

/* ── Forward declarations for forward refs ── */

"""


def gen_pascal_runtime():
    """Runtime support for Pascal-H typed files and non-local termination."""
    with open("TeXSource/TeX.pool", "r", encoding="latin-1") as pool_file:
        pool_lines = pool_file.readlines()
    pool_literal = "\n".join(
        json.dumps(line, ensure_ascii=True) for line in pool_lines)
    return (
        "static const char tex_embedded_pool[] =\n"
        + pool_literal + ";\n"
        + r"""
/* ── Pascal-H runtime ── */
static jmp_buf tex_exit_environment;
static bool tex_exit_environment_ready;
static int tex_command_argc;
static char **tex_command_argv;

static void tex_runtime_set_arguments(int argc, char **argv)
{
    tex_command_argc = argc;
    tex_command_argv = argv;
}

static void tex_jump(int label)
{
    if (tex_exit_environment_ready)
        longjmp(tex_exit_environment, label);
    exit(EXIT_FAILURE);
}

static int odd(int value)
{
    return (value & 1) != 0;
}

static const char *tex_file_name(const char *name)
{
    static char cleaned[1024];
    const char *source = name[0] == '\0' ? name + 1 : name;
    size_t length = strnlen(source, sizeof(cleaned) - 1);

    while (length > 0 && source[length - 1] == ' ')
        --length;
    memcpy(cleaned, source, length);
    cleaned[length] = '\0';
    return cleaned;
}

static const char *tex_resolve_input(const char *logical_name)
{
    static char resolved[2048];
    const char *name = logical_name;
    const char *environment_name = NULL;
    const char *search_path;
    const char *part;
    struct {
        const char *prefix;
        const char *environment;
    } mappings[] = {
        {"TeXfonts:", "TEXFONTS"},
        {"TeXinputs:", "TEXINPUTS"},
        {"TeXformats:", "TEXFORMATS"},
    };
    size_t mapping;

    for (mapping = 0; mapping < sizeof(mappings) / sizeof(mappings[0]);
         ++mapping) {
        size_t prefix_length = strlen(mappings[mapping].prefix);
        if (strncmp(name, mappings[mapping].prefix, prefix_length) == 0) {
            name += prefix_length;
            environment_name = mappings[mapping].environment;
            break;
        }
    }
    if (strchr(name, '/') != NULL || environment_name == NULL)
        return name;
    if (access(name, R_OK) == 0)
        return name;

    search_path = getenv(environment_name);
    if (search_path == NULL)
        return name;
    part = search_path;
    for (;;) {
        const char *separator = strchr(part, ':');
        size_t directory_length = separator == NULL
            ? strlen(part) : (size_t)(separator - part);
        int length;

        if (directory_length == 0)
            length = snprintf(resolved, sizeof(resolved), "%s", name);
        else
            length = snprintf(resolved, sizeof(resolved), "%.*s/%s",
                              (int)directory_length, part, name);
        if (length >= 0 && (size_t)length < sizeof(resolved)
            && access(resolved, R_OK) == 0)
            return resolved;
        if (separator == NULL)
            break;
        part = separator + 1;
    }
    return name;
}

static void tex_text_prime(alphafile *file)
{
    int value = fgetc(file->handle);
    file->current = value;
    file->at_eof = value == EOF;
    file->at_eoln = value == '\n' || value == '\r' || value == EOF;
}

static void tex_text_reset(alphafile *file, const char *name,
                           const char *options)
{
    (void)options;
    memset(file, 0, sizeof(*file));
    if (strncmp(tex_file_name(name), "TTY:", 4) == 0) {
        if (tex_command_argc > 1) {
            int argument;
            file->handle = tmpfile();
            if (file->handle != NULL) {
                for (argument = 1; argument < tex_command_argc; ++argument) {
                    if (argument > 1)
                        fputc(' ', file->handle);
                    fputs(tex_command_argv[argument], file->handle);
                }
                fputc('\n', file->handle);
                rewind(file->handle);
            }
        } else {
            file->handle = stdin;
        }
        file->current = '\n';
        file->at_eoln = true;
        file->synthetic_eol = true;
    } else if (strstr(tex_file_name(name), "TEX.POOL") != NULL) {
        file->handle = tmpfile();
        if (file->handle != NULL) {
            fwrite(tex_embedded_pool, 1, sizeof(tex_embedded_pool) - 1,
                   file->handle);
            rewind(file->handle);
        }
    } else
        file->handle = fopen(tex_resolve_input(tex_file_name(name)), "r");
    file->status = file->handle == NULL ? errno : 0;
    if (file->handle == NULL)
        file->at_eof = file->at_eoln = true;
    else if (!file->synthetic_eol)
        tex_text_prime(file);
}

static void tex_text_rewrite(alphafile *file, const char *name,
                             const char *options)
{
    (void)options;
    memset(file, 0, sizeof(*file));
    if (strncmp(tex_file_name(name), "TTY:", 4) == 0)
        file->handle = stdout;
    else
        file->handle = fopen(tex_file_name(name), "w");
    file->status = file->handle == NULL ? errno : 0;
}

static void tex_byte_prime(bytefile *file)
{
    int value = fgetc(file->handle);
    file->current = value == EOF ? 0 : (uint8_t)value;
    file->at_eof = value == EOF;
}

static void tex_byte_reset(bytefile *file, const char *name,
                           const char *options)
{
    const char *path;
    (void)options;
    memset(file, 0, sizeof(*file));
    path = tex_resolve_input(tex_file_name(name));
#ifdef TEX_DEBUG
    fprintf(stderr, "DEBUG byte reset: [%s]\\n", path);
#endif
    file->handle = fopen(path, "rb");
    file->status = file->handle == NULL ? errno : 0;
    if (file->handle != NULL)
        tex_byte_prime(file);
    else
        file->at_eof = true;
}

static void tex_byte_rewrite(bytefile *file, const char *name,
                             const char *options)
{
    (void)options;
    memset(file, 0, sizeof(*file));
    file->handle = fopen(tex_file_name(name), "wb");
    file->status = file->handle == NULL ? errno : 0;
}

static void tex_word_prime(wordfile *file)
{
    file->at_eof =
        fread(&file->current, sizeof(file->current), 1, file->handle) != 1;
}

static void tex_word_reset(wordfile *file, const char *name,
                           const char *options)
{
    (void)options;
    memset(file, 0, sizeof(*file));
    file->handle = fopen(tex_resolve_input(tex_file_name(name)), "rb");
    file->status = file->handle == NULL ? errno : 0;
    if (file->handle != NULL)
        tex_word_prime(file);
    else
        file->at_eof = true;
}

static void tex_word_rewrite(wordfile *file, const char *name,
                             const char *options)
{
    (void)options;
    memset(file, 0, sizeof(*file));
    file->handle = fopen(tex_file_name(name), "wb");
    file->status = file->handle == NULL ? errno : 0;
}

static void tex_text_get(alphafile *file)
{
    if (file->handle != NULL)
        tex_text_prime(file);
}

static void tex_byte_get(bytefile *file)
{
    if (file->handle != NULL)
        tex_byte_prime(file);
}

static void tex_word_get(wordfile *file)
{
    if (file->handle != NULL)
        tex_word_prime(file);
}

static void tex_word_put(wordfile *file)
{
    if (file->handle != NULL
        && fwrite(&file->current, sizeof(file->current), 1, file->handle) != 1)
        file->status = errno ? errno : EIO;
}

static void tex_text_close(alphafile *file)
{
    if (file->handle != NULL && file->handle != stdin
        && file->handle != stdout && file->handle != stderr)
        fclose(file->handle);
    file->handle = NULL;
}

static void tex_byte_close(bytefile *file)
{
    if (file->handle != NULL)
        fclose(file->handle);
    file->handle = NULL;
}

static void tex_word_close(wordfile *file)
{
    if (file->handle != NULL)
        fclose(file->handle);
    file->handle = NULL;
}

static void tex_text_readln(alphafile *file)
{
    if (file->synthetic_eol) {
        file->synthetic_eol = false;
        return;
    }
    while (!file->at_eof && !file->at_eoln)
        tex_text_get(file);
    if (!file->at_eof) {
        int previous = file->current;
        tex_text_get(file);
        if (previous == '\r' && file->current == '\n')
            tex_text_get(file);
    }
}

static void tex_text_write_char(alphafile *file, int value)
{
    if (file->handle != NULL)
        fputc((unsigned char)value, file->handle);
}

static void tex_text_write_string(alphafile *file, const char *value)
{
    if (file->handle != NULL)
        fputs(value, file->handle);
}

static void tex_text_write_int(alphafile *file, int value, int width)
{
    if (file->handle != NULL)
        fprintf(file->handle, "%*d", width, value);
}

static void tex_text_newline(alphafile *file)
{
    if (file->handle != NULL)
        fputc('\n', file->handle);
}

static void tex_byte_write(bytefile *file, int value)
{
    if (file->handle != NULL
        && fputc((unsigned char)value, file->handle) == EOF)
        file->status = errno ? errno : EIO;
}

static void pascal_break(alphafile file)
{
    if (file.handle != NULL)
        fflush(file.handle);
}

static void breakin(alphafile *file, int ignored)
{
    (void)file;
    (void)ignored;
}

#define reset(file, name, options) _Generic((file), \
    alphafile *: tex_text_reset, bytefile *: tex_byte_reset, \
    wordfile *: tex_word_reset)((file), (name), (options))
#define rewrite(file, name, options) _Generic((file), \
    alphafile *: tex_text_rewrite, bytefile *: tex_byte_rewrite, \
    wordfile *: tex_word_rewrite)((file), (name), (options))
#define erstat(file) ((file)->status)
#define eof(file) ((file)->at_eof)
#define eoln(file) ((file)->at_eoln)
#define get(file) _Generic((file), \
    alphafile *: tex_text_get, bytefile *: tex_byte_get, \
    wordfile *: tex_word_get)(file)
#define put(file) tex_word_put(file)
#define close(file) _Generic((file), \
    alphafile *: tex_text_close, bytefile *: tex_byte_close, \
    wordfile *: tex_word_close)(file)

""")


def gen_tail():
    """C epilogue: stub implementations for forward-declared functions."""
    return "\n/* End of TeX.c */\n"


# ──────────────────────────────────────────────────────────────────
# Forward declaration collection
# ──────────────────────────────────────────────────────────────────

def collect_forward_declarations(block):
    """Collect forward-declared subprograms from top-level and nested blocks."""
    forwards = []

    def walk_block(b, depth=0):
        if depth > 10:
            return
        if isinstance(b, dict):
            for s in b.get("subprograms", []):
                if s.get("forward"):
                    forwards.append(s)
            for s in b.get("subprograms", []):
                if s.get("block") and not s.get("forward"):
                    walk_block(s["block"], depth + 1)
        elif isinstance(b, list):
            for item in b:
                walk_block(item, depth)

    walk_block(block)
    return forwards


# ──────────────────────────────────────────────────────────────────
# All labels in the program
# ──────────────────────────────────────────────────────────────────

def collect_all_labels(block):
    """Collect all goto labels from Block nodes only (not CaseArm labels)."""
    labels = set()

    def walk(node, depth=0):
        if depth > 30:
            return
        if isinstance(node, dict):
            # Block labels are integer goto labels; CaseArm labels are Literal dicts
            if "labels" in node and isinstance(node["labels"], list) and len(node["labels"]) > 0:
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


# ──────────────────────────────────────────────────────────────────
# Type map population
# ──────────────────────────────────────────────────────────────────

def build_type_map(block):
    """Build a map of Pascal type names to C type strings from all type declarations."""
    type_map = {}
    # Pre-populate primitive types
    for p in PRIMITIVE_TYPES:
        type_map[p] = "int"

    def walk(node, depth=0):
        if depth > 20:
            return
        if isinstance(node, dict):
            kind = node.get("kind")
            if kind == "TypeDecl" and "name" in node and "type" in node:
                name = node["name"]
                c_type = pascal_type_to_c(node["type"], type_map)
                type_map[name] = c_type
            elif kind == "ArrayType" or kind == "RecordType" or kind == "FileType":
                # These need recursion
                pass
            for v in node.values():
                walk(v, depth + 1)
        elif isinstance(node, list):
            for item in node:
                walk(item, depth + 1)

    # Walk top-level types
    for td in block.get("types", []):
        name = td["name"]
        c_type = pascal_type_to_c(td["type"], type_map)
        type_map[name] = c_type

    # Walk subprogram types too
    def walk_block_types(b):
        if not isinstance(b, dict):
            return
        for td in b.get("types", []):
            name = td["name"]
            c_type = pascal_type_to_c(td["type"], type_map)
            type_map[name] = c_type
        for s in b.get("subprograms", []):
            if s.get("block"):
                walk_block_types(s["block"])

    walk_block_types(block)

    # Also handle VarDecl types (arrays, etc.)
    def walk_block_vars(b):
        if not isinstance(b, dict):
            return
        for vd in b.get("variables", []):
            t = vd.get("type")
            if t:
                # Ensure the element type is in the map
                _ensure_type(t, type_map)
        for s in b.get("subprograms", []):
            if s.get("block"):
                walk_block_vars(s["block"])

    def _ensure_type(t, tm):
        if not t or not isinstance(t, dict):
            return
        k = t.get("kind")
        if k == "NamedType":
            if t["name"] not in tm:
                tm[t["name"]] = f"/* {t['name']} */ int"
        elif k == "ArrayType":
            _ensure_type(t.get("element_type"), tm)
            for idx in t.get("index_types", []):
                _ensure_type(idx, tm)
        elif k == "RecordType":
            pass  # records handled by struct generation
        elif k == "FileType":
            pass
        elif k == "SubrangeType":
            pass  # subrange types are int-based
        elif k == "PointerType":
            _ensure_type(t.get("base"), tm)

    walk_block_vars(block)

    return type_map


# ──────────────────────────────────────────────────────────────────
# Main generator
# ──────────────────────────────────────────────────────────────────

def generate_c(doc):
    """Generate complete C source from the parsed AST document."""
    global FUNCTION_NAMES, SUBPROGRAM_PARAMS, GLOBAL_SCOPE_TYPES
    block = doc["block"]
    FUNCTION_NAMES = {
        subprogram["name"]
        for subprogram in block.get("subprograms", [])
        if subprogram.get("category") == "function"
    }
    SUBPROGRAM_PARAMS = {
        subprogram["name"]: subprogram.get("parameters", [])
        for subprogram in block.get("subprograms", [])
    }
    GLOBAL_SCOPE_TYPES = {}
    for variable in block.get("variables", []):
        variable_type = variable.get("type", {})
        if variable_type.get("kind") == "NamedType":
            type_name = variable_type["name"]
        elif (variable_type.get("kind") == "ArrayType"
              and variable_type.get("element_type", {}).get("kind")
              == "NamedType"):
            type_name = variable_type["element_type"]["name"]
        else:
            type_name = None
        for variable_name in variable.get("names", []):
            GLOBAL_SCOPE_TYPES[variable_name] = type_name

    # Build type map
    type_map = build_type_map(block)

    # Collect all labels
    all_labels = collect_all_labels(block)

    # Collect forward declarations
    forwards = collect_forward_declarations(block)

    # Start with header
    lines = [gen_header()]

    # Type declarations (structs/unions first)
    lines.append("/* ── Type definitions ── */\n")
    for td in block.get("types", []):
        kind = td["type"].get("kind")
        if kind == "RecordType":
            lines.append(generate_record_cdef(td["type"], td["name"], type_map))
        else:
            lines.append(gen_type_decl(td, type_map))

    # Local types from subprograms (structs)
    def walk_and_emit_types(b):
        if not isinstance(b, dict):
            return
        for td in b.get("types", []):
            kind = td["type"].get("kind")
            if kind == "RecordType":
                lines.append(generate_record_cdef(td["type"], td["name"], type_map))
            else:
                lines.append(gen_type_decl(td, type_map))
        for s in b.get("subprograms", []):
            if s.get("block"):
                walk_and_emit_types(s["block"])

    for subprogram in block.get("subprograms", []):
        if subprogram.get("block"):
            walk_and_emit_types(subprogram["block"])

    lines.append("\n")
    lines.append(gen_pascal_runtime())

    # Constants as #defines
    lines.append("/* ── Constants ── */\n")
    for cd in block.get("constants", []):
        lines.append(gen_const_decl(cd, type_map))
    lines.append("\n")

    # Global variable declarations
    lines.append("/* ── Global variables ── */\n")
    for vd in block.get("variables", []):
        lines.append(gen_var_decl(vd, type_map))
    lines.append("\n")

    # C prototypes for all Pascal routines (which may call later routines).
    lines.append("/* ── Forward declarations ── */\n")
    emitted_prototypes = set()
    for subprogram in block.get("subprograms", []):
        name = subprogram["name"]
        if name in emitted_prototypes:
            continue
        emitted_prototypes.add(name)
        parameters = ", ".join(
            gen_param_decl(parameter, type_map)
            for parameter in subprogram.get("parameters", []))
        result_type = pascal_type_to_c(
            subprogram.get("return_type"), type_map
        ) if subprogram.get("return_type") else "void"
        lines.append(f"{result_type} {name}({parameters});\n")
    lines.append("\n")

    # Generate subprograms
    lines.append("/* ── Subprograms ── */\n")
    for sp in block.get("subprograms", []):
        if sp.get("forward"):
            continue  # Already emitted as forward decl
        lines.append(gen_subprogram(sp, type_map, all_labels))

    # Main function
    lines.append(gen_main_body(block, type_map, all_labels))

    # Epilogue
    lines.append(gen_tail())

    return "".join(lines)


# ──────────────────────────────────────────────────────────────────
# Entry point
# ──────────────────────────────────────────────────────────────────

def main():
    yaml_path = sys.argv[1] if len(sys.argv) > 1 else "TeX.yaml"
    output_path = sys.argv[2] if len(sys.argv) > 2 else "TeX.c"

    print(f"Loading {yaml_path}...", file=sys.stderr)
    with open(yaml_path, "r") as f:
        doc = yaml.safe_load(f)

    print("Validating AST...", file=sys.stderr)
    validate_schema(doc)

    print(f"Generating C source...", file=sys.stderr)
    c_source = generate_c(doc)

    print(f"Writing {output_path}...", file=sys.stderr)
    with open(output_path, "w") as f:
        f.write(c_source)

    print(f"Done. Generated {len(c_source)} bytes → {output_path}", file=sys.stderr)


if __name__ == "__main__":
    main()
