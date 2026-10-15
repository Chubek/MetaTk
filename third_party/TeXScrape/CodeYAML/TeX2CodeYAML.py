#!/usr/bin/env python3
"""
tex_to_yaml.py -- Convert the Pascal-W program in `tex.p` (Knuth's TeX82,
TANGLE output) into a Concrete AST expressed as YAML (`TeX.yaml`).

AGENTS.md asks us to:
  * extract the Pascal-W program encoded in tex.p, and
  * rewrite it in YAML as a "Concrete AST", refining the supplied schema.

Design notes
------------
* The source is TANGLE output.  It carries two flavours of WEB section
  markers interleaved with the Pascal text:
      {NN:}  /  {:NN}      (brace form, 1285 each)
      [NN:]  /  [:NN]      (bracket form, 22 each)
  and compiler directives / comments in braces  {....}  /  (* .... *).
  All of these are *trivia*: they are not Pascal and are elided by the
  lexer so the parser sees clean Pascal.  (Section markers could be
  re-attached as trivia later; the supplied example schema carries none.)

* TANGLE strips spaces, so tokens abut: `)or(`, `free[p]then`,
  `mem[p+1].hh.rh>=lomemmax`.  The lexer is therefore character based and
  does not rely on whitespace.

Schema (refined from AGENTS.md)
-------------------------------
The example schema had three problems we fix:
  1. `signature` was a list of single-key dicts ("- returns:" then
     "- formals:") -- made a proper object with `return_type`/`parameters`.
  2. `statement[N]` numbering duplicated the list index -- dropped.
  3. Expressions flattened `a+b-1` into `constants:[a,b,1]`+`operators:[+,-]`,
     losing the tree.  We emit a real recursive expression tree
     (BinaryOp / UnaryOp / Literal / Identifier / Call / Index / FieldAccess
     / Deref).

Every node is a dict with a `kind`.  Source `line` is attached to
declarations and statements for traceability.
"""

import sys
import re
import yaml

SRC = "tex.p"
OUT = "TeX.yaml"

# --------------------------------------------------------------------------
# Lexer
# --------------------------------------------------------------------------

KEYWORDS = {
    "program", "label", "const", "type", "var", "procedure", "function",
    "begin", "end", "if", "then", "else", "while", "do", "repeat", "until",
    "for", "to", "downto", "case", "of", "with", "goto", "forward",
    "packed", "record", "array", "set", "file", "nil", "and", "or", "not",
    "in", "div", "mod", "others", "true", "false",
}

TWO_SYM = {":=", "<=", ">=", "<>", ".."}
ONE_SYM = set("().,;:^[]+-*/=<>")


class Tok:
    __slots__ = ("kind", "text", "value", "line", "col")

    def __init__(self, kind, text, value, line, col):
        self.kind = kind        # 'int','real','string','ident','kw','sym','eof'
        self.text = text        # source text (keywords lower-cased)
        self.value = value      # decoded value for literals
        self.line = line
        self.col = col

    def __repr__(self):
        return f"Tok({self.kind},{self.text!r},L{self.line})"


_BRACKET_MARKER = re.compile(r"\[[0-9]+:\]|\[:[0-9]+\]")


def tokenize(s):
    toks = []
    i = 0
    n = len(s)
    line = 1
    col = 1
    line_start = 0  # index of start of current line

    def adv(k):
        nonlocal i, line, col, line_start
        for _ in range(k):
            if s[i] == "\n":
                line += 1
                col = 1
                line_start = i + 1
            else:
                col += 1
            i += 1

    while i < n:
        c = s[i]
        # whitespace
        if c in " \t\r\n":
            adv(1)
            continue
        # brace comment / directive / WEB marker  {....}
        if c == "{":
            j = s.find("}", i + 1)
            if j == -1:
                raise SyntaxError(f"unterminated {{ comment at line {line}")
            adv(j - i + 1)
            continue
        # (* ... *) comment
        if c == "(" and i + 1 < n and s[i + 1] == "*":
            j = s.find("*)", i + 2)
            if j == -1:
                raise SyntaxError(f"unterminated (* comment at line {line}")
            adv(j - i + 2)
            continue
        # bracket WEB markers  [NN:] / [:NN]
        m = _BRACKET_MARKER.match(s, i)
        if m:
            adv(m.end() - m.start())
            continue
        # number
        if c.isdigit():
            start = i
            sc = col
            ln = line
            while i < n and s[i].isdigit():
                adv(1)
            # real?  digit . digit  (but not  ..  which is a subrange)
            if i < n and s[i] == "." and i + 1 < n and s[i + 1] != "." and s[i + 1].isdigit():
                adv(1)  # dot
                while i < n and s[i].isdigit():
                    adv(1)
                if i < n and s[i] in "eE":
                    adv(1)
                    if i < n and s[i] in "+-":
                        adv(1)
                    while i < n and s[i].isdigit():
                        adv(1)
                toks.append(Tok("real", s[start:i], float(s[start:i]), ln, sc))
            else:
                toks.append(Tok("int", s[start:i], int(s[start:i]), ln, sc))
            continue
        # string literal
        if c == "'":
            ln = line
            sc = col
            adv(1)
            buf = []
            while i < n:
                if s[i] == "'":
                    if i + 1 < n and s[i + 1] == "'":
                        buf.append("'")
                        adv(2)
                        continue
                    adv(1)
                    break
                buf.append(s[i])
                adv(1)
            else:
                raise SyntaxError(f"unterminated string at line {ln}")
            toks.append(Tok("string", "".join(buf), "".join(buf), ln, sc))
            continue
        # identifier / keyword
        if c.isalpha() or c == "_":
            start = i
            sc = col
            ln = line
            while i < n and (s[i].isalnum() or s[i] == "_"):
                adv(1)
            text = s[start:i]
            low = text.lower()
            if low in KEYWORDS:
                toks.append(Tok("kw", low, low, ln, sc))
            else:
                toks.append(Tok("ident", text, text, ln, sc))
            continue
        # two-char symbols
        two = s[i:i + 2]
        if two in TWO_SYM:
            toks.append(Tok("sym", two, two, line, col))
            adv(2)
            continue
        # one-char symbols (note: '[' handled as marker-or-sym above)
        if c in ONE_SYM:
            toks.append(Tok("sym", c, c, line, col))
            adv(1)
            continue
        raise SyntaxError(f"unknown char {c!r} at line {line} col {col}")
    toks.append(Tok("eof", "", "", line, col))
    return toks


# --------------------------------------------------------------------------
# Parser  (recursive descent)
# --------------------------------------------------------------------------

RELOP = {"=", "<>", "<", "<=", ">", ">=", "in"}
ADDOP = {"+", "-", "or"}
MULOP = {"*", "/", "div", "mod", "and"}


class Parser:
    def __init__(self, toks):
        self.toks = toks
        self.pos = 0

    # -- token helpers ----------------------------------------------------
    def peek(self, k=0):
        return self.toks[self.pos + k]

    def next(self):
        t = self.toks[self.pos]
        self.pos += 1
        return t

    def at(self, text):
        t = self.peek()
        return t.text == text

    def at_kw(self, *kws):
        t = self.peek()
        return t.kind == "kw" and t.text in kws

    def at_sym(self, *syms):
        t = self.peek()
        return t.kind == "sym" and t.text in syms

    def expect_sym(self, sym):
        t = self.peek()
        if t.kind == "sym" and t.text == sym:
            return self.next()
        raise SyntaxError(f"expected {sym!r} but got {t.text!r} (L{t.line})")

    def expect_kw(self, kw):
        t = self.peek()
        if t.kind == "kw" and t.text == kw:
            return self.next()
        raise SyntaxError(f"expected keyword {kw!r} but got {t.text!r} (L{t.line})")

    def expect_ident(self):
        t = self.peek()
        if t.kind == "ident":
            return self.next()
        raise SyntaxError(f"expected identifier but got {t.text!r} (L{t.line})")

    # -- program ----------------------------------------------------------
    def parse_program(self):
        self.expect_kw("program")
        name = self.expect_ident().text
        # optional program parameters (input,output) -- TeX has none, but handle
        if self.at_sym("("):
            self.next()
            self.expect_ident()
            while self.at_sym(","):
                self.next()
                self.expect_ident()
            self.expect_sym(")")
        self.expect_sym(";")
        block = self.parse_block(path_prefix=name)
        self.expect_sym(".")
        if self.peek().kind != "eof":
            t = self.peek()
            raise SyntaxError(f"trailing tokens after program end: {t.text!r} (L{t.line})")
        return {
            "kind": "Program",
            "name": name,
            "path": name,
            "schema": "ConcreteAST-PascalW/1",
            "block": block,
        }

    # -- block (label/const/type/var/proc in any order, then body) -------
    def parse_block(self, path_prefix):
        labels = []
        consts = []
        types = []
        variables = []
        subprograms = []
        while True:
            if self.at_kw("label"):
                labels.extend(self.parse_label_part())
            elif self.at_kw("const"):
                consts.extend(self.parse_const_part())
            elif self.at_kw("type"):
                types.extend(self.parse_type_part())
            elif self.at_kw("var"):
                variables.extend(self.parse_var_part())
            elif self.at_kw("procedure", "function"):
                sp = self.parse_proc_decl(path_prefix)
                subprograms.append(sp)
            elif self.at_kw("begin"):
                break
            else:
                t = self.peek()
                raise SyntaxError(f"unexpected token in block: {t.text!r} (L{t.line})")
        body = self.parse_compound()
        return {
            "labels": labels,
            "constants": consts,
            "types": types,
            "variables": variables,
            "subprograms": subprograms,
            "body": body,
        }

    # -- declaration parts ------------------------------------------------
    def parse_label_part(self):
        self.expect_kw("label")
        labels = [self.parse_label_value()]
        while self.at_sym(","):
            self.next()
            labels.append(self.parse_label_value())
        self.expect_sym(";")
        return labels

    def parse_label_value(self):
        t = self.peek()
        if t.kind == "int":
            self.next()
            return t.value
        if t.kind == "ident":
            self.next()
            return t.text
        raise SyntaxError(f"expected label but got {t.text!r} (L{t.line})")

    def parse_const_part(self):
        self.expect_kw("const")
        items = []
        while self.peek().kind == "ident":
            name = self.expect_ident()
            self.expect_sym("=")
            val = self.parse_expression()
            self.expect_sym(";")
            items.append({"kind": "ConstDecl", "name": name.text,
                          "value": val, "line": name.line})
        return items

    def parse_type_part(self):
        self.expect_kw("type")
        items = []
        while self.peek().kind == "ident":
            name = self.expect_ident()
            self.expect_sym("=")
            typ = self.parse_type()
            self.expect_sym(";")
            items.append({"kind": "TypeDecl", "name": name.text,
                          "type": typ, "line": name.line})
        return items

    def parse_var_part(self):
        self.expect_kw("var")
        items = []
        while self.peek().kind == "ident":
            first = self.expect_ident()
            names = [first.text]
            while self.at_sym(","):
                self.next()
                names.append(self.expect_ident().text)
            self.expect_sym(":")
            typ = self.parse_type()
            self.expect_sym(";")
            items.append({"kind": "VarDecl", "names": names,
                          "type": typ, "line": first.line})
        return items

    def parse_proc_decl(self, path_prefix):
        kw = self.next()  # procedure | function
        is_func = kw.text == "function"
        name = self.expect_ident()
        path = f"{path_prefix}.{name.text}"
        params = []
        if self.at_sym("("):
            params = self.parse_param_list()
        return_type = None
        if is_func and self.at_sym(":"):
            self.next()
            return_type = self.parse_type()
        self.expect_sym(";")
        if self.at_kw("forward"):
            self.next()
            self.expect_sym(";")
            return {
                "kind": "Subprogram",
                "name": name.text,
                "path": path,
                "category": kw.text,
                "parameters": params,
                "return_type": return_type,
                "forward": True,
                "block": None,
                "line": name.line,
            }
        block = self.parse_block(path_prefix=path)
        self.expect_sym(";")
        return {
            "kind": "Subprogram",
            "name": name.text,
            "path": path,
            "category": kw.text,
            "parameters": params,
            "return_type": return_type,
            "forward": False,
            "block": block,
            "line": name.line,
        }

    def parse_param_list(self):
        self.expect_sym("(")
        params = []
        while not self.at_sym(")"):
            group = "value"
            if self.at_kw("var"):
                self.next()
                group = "var"
            elif self.at_kw("procedure"):
                self.next()
                group = "procedure"
            elif self.at_kw("function"):
                self.next()
                group = "function"
            first = self.expect_ident()
            names = [first.text]
            while self.at_sym(","):
                self.next()
                names.append(self.expect_ident().text)
            ptype = None
            if group == "procedure":
                # may have its own param list; ignore detail, no return type
                if self.at_sym("("):
                    self.skip_balanced_parens()
            else:
                self.expect_sym(":")
                ptype = self.parse_type()
            for nm in names:
                params.append({"kind": "ParamDecl", "name": nm,
                               "type": ptype, "mode": group, "line": first.line})
            if self.at_sym(";"):
                self.next()
                continue
            break
        self.expect_sym(")")
        return params

    def skip_balanced_parens(self):
        # used only for rare procedure/function formal params
        self.expect_sym("(")
        depth = 1
        while depth > 0:
            t = self.next()
            if t.kind == "eof":
                raise SyntaxError("EOF in formal param list")
            if t.kind == "sym" and t.text == "(":
                depth += 1
            elif t.kind == "sym" and t.text == ")":
                depth -= 1

    # -- types ------------------------------------------------------------
    def parse_type(self):
        if self.at_sym("^"):
            self.next()
            return {"kind": "PointerType", "base": self.parse_type()}
        packed = False
        if self.at_kw("packed"):
            self.next()
            packed = True
        if self.at_kw("array"):
            self.next()
            self.expect_sym("[")
            idx = [self.parse_type()]
            while self.at_sym(","):
                self.next()
                idx.append(self.parse_type())
            self.expect_sym("]")
            self.expect_kw("of")
            elem = self.parse_type()
            return {"kind": "ArrayType", "index_types": idx,
                    "element_type": elem, "packed": packed}
        if self.at_kw("record"):
            self.next()
            fl = self.parse_field_list()
            self.expect_kw("end")
            return {"kind": "RecordType", "fixed": fl["fixed"],
                    "variant": fl["variant"], "packed": packed}
        if self.at_kw("set"):
            self.next()
            self.expect_kw("of")
            return {"kind": "SetType", "element_type": self.parse_type(),
                    "packed": packed}
        if self.at_kw("file"):
            self.next()
            self.expect_kw("of")
            return {"kind": "FileType", "element_type": self.parse_type(),
                    "packed": packed}
        if self.at_sym("("):
            self.next()
            vals = [self.expect_ident().text]
            while self.at_sym(","):
                self.next()
                vals.append(self.expect_ident().text)
            self.expect_sym(")")
            return {"kind": "EnumeratedType", "values": vals}
        # named type or subrange
        c = self.parse_const_value()
        if self.at_sym(".."):
            self.next()
            hi = self.parse_const_value()
            return {"kind": "SubrangeType", "lower": c, "upper": hi}
        if c.get("kind") != "Identifier":
            t = self.peek()
            raise SyntaxError(f"expected a type name but got {c} (L{t.line})")
        return {"kind": "NamedType", "name": c["name"]}

    def parse_const_value(self):
        neg = None
        if self.at_sym("+", "-"):
            neg = self.next().text
        t = self.peek()
        if t.kind == "int":
            self.next()
            v = {"kind": "Literal", "value": t.value, "literal_type": "integer"}
        elif t.kind == "real":
            self.next()
            v = {"kind": "Literal", "value": t.value, "literal_type": "real"}
        elif t.kind == "string":
            self.next()
            v = {"kind": "Literal", "value": t.value, "literal_type": "string"}
        elif t.kind == "ident":
            self.next()
            v = {"kind": "Identifier", "name": t.text}
        else:
            raise SyntaxError(f"expected a constant but got {t.text!r} (L{t.line})")
        if neg == "-":
            v = {"kind": "UnaryOp", "operator": "-", "operand": v}
        elif neg == "+":
            v = {"kind": "UnaryOp", "operator": "+", "operand": v}
        return v

    def parse_field_list(self):
        fixed = []
        variant = None
        if self.at_kw("case"):
            variant = self.parse_variant_part()
        else:
            fixed.append(self.parse_record_section())
            while self.at_sym(";"):
                self.next()
                if self.at_kw("end"):
                    break
                if self.at_kw("case"):
                    variant = self.parse_variant_part()
                    break
                fixed.append(self.parse_record_section())
        return {"fixed": fixed, "variant": variant}

    def parse_record_section(self):
        first = self.expect_ident()
        names = [first.text]
        while self.at_sym(","):
            self.next()
            names.append(self.expect_ident().text)
        self.expect_sym(":")
        typ = self.parse_type()
        return {"fields": names, "type": typ, "line": first.line}

    def parse_variant_part(self):
        self.expect_kw("case")
        tag = self.parse_tag()
        self.expect_kw("of")
        variants = []
        while True:
            labels = [self.parse_case_label()]
            while self.at_sym(","):
                self.next()
                labels.append(self.parse_case_label())
            self.expect_sym(":")
            self.expect_sym("(")
            fl = self.parse_field_list()
            self.expect_sym(")")
            variants.append({"labels": labels, "fields": fl})
            if self.at_sym(";"):
                self.next()
                if self.at_kw("end"):
                    break
                continue
            break
        return {"tag": tag, "variants": variants}

    def parse_tag(self):
        if self.peek().kind == "ident" and self.peek(1).kind == "sym" and self.peek(1).text == ":":
            name = self.next().text
            self.expect_sym(":")
            typ = self.parse_type()
            return {"name": name, "type": typ}
        return {"name": None, "type": self.parse_type()}

    def parse_case_label(self):
        neg = None
        if self.at_sym("+", "-"):
            neg = self.next().text
        t = self.peek()
        if t.kind == "int":
            self.next()
            v = {"kind": "Literal", "value": t.value, "literal_type": "integer"}
        elif t.kind == "ident":
            self.next()
            v = {"kind": "Identifier", "name": t.text}
        elif t.kind == "string":
            self.next()
            v = {"kind": "Literal", "value": t.value, "literal_type": "string"}
        else:
            raise SyntaxError(f"expected case label but got {t.text!r} (L{t.line})")
        if neg == "-":
            v = {"kind": "UnaryOp", "operator": "-", "operand": v}
        elif neg == "+":
            v = {"kind": "UnaryOp", "operator": "+", "operand": v}
        if self.at_sym(".."):
            self.next()
            hi = self.parse_case_label()
            return {"kind": "Range", "lower": v, "upper": hi}
        return v

    # -- statements -------------------------------------------------------
    def parse_compound(self):
        kw = self.expect_kw("begin")
        stmts = self.parse_stmt_list({"end"})
        self.expect_kw("end")
        return {"kind": "Compound", "statements": stmts, "line": kw.line}

    def parse_stmt_list(self, stop):
        stmts = []
        while True:
            t = self.peek()
            if t.kind == "eof":
                break
            if t.kind == "kw" and t.text in stop:
                break
            if t.kind == "sym" and t.text == ";":
                self.next()
                continue
            stmts.append(self.parse_statement())
            if self.at_sym(";"):
                self.next()
            else:
                break
        return stmts

    def parse_statement(self):
        t = self.peek()
        # labeled statement
        if t.kind == "int":
            self.next()
            self.expect_sym(":")
            stmt = self.parse_statement()
            return {"kind": "Labeled", "label": t.value, "statement": stmt,
                    "line": t.line}
        if t.kind == "kw":
            if t.text == "begin":
                return self.parse_compound()
            if t.text == "if":
                return self.parse_if()
            if t.text == "while":
                return self.parse_while()
            if t.text == "repeat":
                return self.parse_repeat()
            if t.text == "for":
                return self.parse_for()
            if t.text == "case":
                return self.parse_case_stmt()
            if t.text == "with":
                return self.parse_with()
            if t.text == "goto":
                self.next()
                lab = self.parse_label_value()
                return {"kind": "Goto", "label": lab, "line": t.line}
            # end/until/else/else-of -> empty statement
            return {"kind": "Empty", "line": t.line}
        if t.kind == "ident":
            start_line = t.line
            desig = self.parse_designator()
            if self.at_sym(":="):
                self.next()
                val = self.parse_expression()
                return {"kind": "Assignment", "target": desig, "value": val,
                        "line": start_line}
            # procedure call
            return self.proc_call_from(desig, start_line)
        if t.kind == "eof":
            return {"kind": "Empty", "line": t.line}
        # empty statement: ';' terminator with nothing before it
        if t.kind == "sym" and t.text == ";":
            return {"kind": "Empty", "line": t.line}
        raise SyntaxError(f"unexpected token in statement: {t.text!r} (L{t.line})")

    def proc_call_from(self, desig, line):
        if desig["kind"] == "Identifier":
            return {"kind": "ProcedureCall", "name": desig["name"],
                    "arguments": [], "line": line}
        if desig["kind"] == "Call" and desig["base"]["kind"] == "Identifier":
            return {"kind": "ProcedureCall", "name": desig["base"]["name"],
                    "arguments": desig["arguments"], "line": line}
        raise SyntaxError(f"invalid procedure call statement: {desig} (L{line})")

    def parse_if(self):
        kw = self.expect_kw("if")
        cond = self.parse_expression()
        self.expect_kw("then")
        then = self.parse_statement()
        els = None
        if self.at_kw("else"):
            self.next()
            els = self.parse_statement()
        return {"kind": "If", "condition": cond, "then": then, "else": els,
                "line": kw.line}

    def parse_while(self):
        kw = self.expect_kw("while")
        cond = self.parse_expression()
        self.expect_kw("do")
        body = self.parse_statement()
        return {"kind": "While", "condition": cond, "body": body, "line": kw.line}

    def parse_repeat(self):
        kw = self.expect_kw("repeat")
        stmts = self.parse_stmt_list({"until"})
        self.expect_kw("until")
        cond = self.parse_expression()
        return {"kind": "Repeat", "statements": stmts, "condition": cond,
                "line": kw.line}

    def parse_for(self):
        kw = self.expect_kw("for")
        var = self.expect_ident()
        self.expect_sym(":=")
        init = self.parse_expression()
        if self.at_kw("to"):
            self.next()
            direction = "to"
        elif self.at_kw("downto"):
            self.next()
            direction = "downto"
        else:
            t = self.peek()
            raise SyntaxError(f"expected to/downto but got {t.text!r} (L{t.line})")
        final = self.parse_expression()
        self.expect_kw("do")
        body = self.parse_statement()
        return {"kind": "For", "variable": var.text, "direction": direction,
                "initial": init, "final": final, "body": body, "line": kw.line}

    def parse_with(self):
        kw = self.expect_kw("with")
        recs = [self.parse_designator()]
        while self.at_sym(","):
            self.next()
            recs.append(self.parse_designator())
        self.expect_kw("do")
        body = self.parse_statement()
        return {"kind": "With", "records": recs, "body": body, "line": kw.line}

    def parse_case_stmt(self):
        kw = self.expect_kw("case")
        sel = self.parse_expression()
        self.expect_kw("of")
        arms = []
        while True:
            t = self.peek()
            if t.kind == "eof" or (t.kind == "kw" and t.text == "end"):
                break
            if t.kind == "sym" and t.text == ";":
                self.next()
                continue
            if self.at_kw("others"):
                self.next()
                self.expect_sym(":")
                body = self.parse_statement()
                arms.append({"labels": "others", "statement": body})
            else:
                labels = [self.parse_case_label()]
                while self.at_sym(","):
                    self.next()
                    labels.append(self.parse_case_label())
                self.expect_sym(":")
                body = self.parse_statement()
                arms.append({"labels": labels, "statement": body})
            if self.at_sym(";"):
                self.next()
            else:
                break
        self.expect_kw("end")
        return {"kind": "Case", "selector": sel, "arms": arms, "line": kw.line}

    # -- expressions ------------------------------------------------------
    def parse_expression(self):
        left = self.parse_simple_expr()
        while self.peek().kind == "sym" and self.peek().text in RELOP or \
              (self.peek().kind == "kw" and self.peek().text == "in"):
            op = self.next().text
            right = self.parse_simple_expr()
            left = {"kind": "BinaryOp", "operator": op, "left": left, "right": right}
        return left

    def parse_simple_expr(self):
        sign = None
        if self.at_sym("+", "-"):
            sign = self.next().text
        left = self.parse_term()
        if sign:
            left = {"kind": "UnaryOp", "operator": sign, "operand": left}
        while (self.peek().kind == "sym" and self.peek().text in ADDOP) or \
              (self.peek().kind == "kw" and self.peek().text == "or"):
            op = self.next().text
            right = self.parse_term()
            left = {"kind": "BinaryOp", "operator": op, "left": left, "right": right}
        return left

    def parse_term(self):
        left = self.parse_factor()
        while (self.peek().kind == "sym" and self.peek().text in MULOP) or \
              (self.peek().kind == "kw" and self.peek().text in ("and", "div", "mod")):
            op = self.next().text
            right = self.parse_factor()
            left = {"kind": "BinaryOp", "operator": op, "left": left, "right": right}
        return left

    def parse_factor(self):
        t = self.peek()
        if t.kind == "kw" and t.text == "not":
            self.next()
            return {"kind": "UnaryOp", "operator": "not", "operand": self.parse_factor()}
        if t.kind == "kw" and t.text in ("true", "false", "nil"):
            self.next()
            lt = {"true": "boolean", "false": "boolean", "nil": "nil"}[t.text]
            return {"kind": "Literal", "value": (t.text == "true") if lt == "boolean" else None,
                    "literal_type": lt}
        if t.kind == "int":
            self.next()
            return {"kind": "Literal", "value": t.value, "literal_type": "integer"}
        if t.kind == "real":
            self.next()
            return {"kind": "Literal", "value": t.value, "literal_type": "real"}
        if t.kind == "string":
            self.next()
            return {"kind": "Literal", "value": t.value, "literal_type": "string"}
        if t.kind == "sym" and t.text == "(":
            self.next()
            e = self.parse_expression()
            self.expect_sym(")")
            return e
        if t.kind == "ident":
            return self.parse_designator()
        raise SyntaxError(f"unexpected token in expression: {t.text!r} (L{t.line})")

    def parse_write_arg(self):
        # write/writeln argument: expr [ ':' width [ ':' decimals ] ]
        e = self.parse_expression()
        if self.at_sym(":"):
            self.next()
            width = self.parse_expression()
            decimals = None
            if self.at_sym(":"):
                self.next()
                decimals = self.parse_expression()
            return {"kind": "WriteField", "value": e, "width": width,
                    "decimals": decimals}
        return e

    def parse_designator(self):
        name = self.expect_ident()
        node = {"kind": "Identifier", "name": name.text}
        while True:
            t = self.peek()
            if t.kind == "sym" and t.text == ".":
                self.next()
                fld = self.expect_ident()
                node = {"kind": "FieldAccess", "base": node, "field": fld.text}
            elif t.kind == "sym" and t.text == "^":
                self.next()
                node = {"kind": "Deref", "base": node}
            elif t.kind == "sym" and t.text == "[":
                self.next()
                idx = [self.parse_expression()]
                while self.at_sym(","):
                    self.next()
                    idx.append(self.parse_expression())
                self.expect_sym("]")
                node = {"kind": "Index", "base": node, "indices": idx}
            elif t.kind == "sym" and t.text == "(":
                self.next()
                args = [self.parse_write_arg()]
                while self.at_sym(","):
                    self.next()
                    args.append(self.parse_write_arg())
                self.expect_sym(")")
                node = {"kind": "Call", "base": node, "arguments": args}
            else:
                break
        return node


# --------------------------------------------------------------------------
# Output: prune None values, emit YAML
# --------------------------------------------------------------------------

def prune_none(obj):
    if isinstance(obj, dict):
        return {k: prune_none(v) for k, v in obj.items() if v is not None}
    if isinstance(obj, list):
        return [prune_none(v) for v in obj]
    return obj


def main():
    with open(SRC, "r", encoding="latin-1") as f:
        src = f.read()
    toks = tokenize(src)
    p = Parser(toks)
    ast = p.parse_program()
    if p.peek().kind != "eof":
        t = p.peek()
        raise SyntaxError(f"parser did not consume all input; stuck at {t.text!r} (L{t.line})")
    ast = prune_none(ast)

    # a couple of summary stats for transparency
    class Dumper(yaml.SafeDumper):
        pass

    def represent_none(dumper, _):
        return dumper.represent_scalar("tag:yaml.org,2002:null", "")

    with open(OUT, "w", encoding="utf-8") as f:
        yaml.dump(ast, f, Dumper=yaml.SafeDumper, default_flow_style=False,
                  sort_keys=False, width=1000, allow_unicode=True)
    # quick stdout summary
    def count(node, key, acc):
        if isinstance(node, dict):
            if node.get("kind") == key:
                acc[0] += 1
            for v in node.values():
                count(v, key, acc)
        elif isinstance(node, list):
            for v in node:
                count(v, key, acc)
    for k in ("Subprogram", "Assignment", "If", "While", "For", "Repeat",
              "Case", "Compound", "Goto", "VarDecl", "TypeDecl", "ConstDecl"):
        c = [0]
        count(ast, k, c)
        print(f"  {k:14s} {c[0]}")
    print(f"Wrote {OUT}")


if __name__ == "__main__":
    main()
