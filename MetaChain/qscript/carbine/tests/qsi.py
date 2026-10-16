#!/usr/bin/env python3
"""qsi: a small QScript interpreter for testing the Carbine engine.

This is NOT the QScript VM. It implements the language subset the Carbine
engine uses, following qscript/grammar/QScript.grammar and the examples in
the QScript tree, so that the engine's logic can be exercised without a
QScript build. Differences from the real VM are possible; anything the
engine relies on is kept to the common subset:

  * 64-bit wrapping integers; `/` and `%` truncate toward zero
  * `use a, b::c;` makes public fn/const names visible unqualified, or as
    a::f / b::c::f; libraries are looked up as DIR/NAME.qs (:: -> /)
  * functions see their own locals and the module's consts, never the
    program's top-level `let` bindings
  * `carbine::NAME(...)` calls go to the native table given by the caller
  * `return` at top level halts the program with that result
"""

import os
import re
import sys

MASK = (1 << 64) - 1
KEYWORDS = {
    "use", "fn", "sub", "const", "let", "my", "if", "elsif", "else", "unless",
    "while", "until", "do", "for", "loop", "break", "last", "continue", "next",
    "return", "exit", "print", "say", "assert", "and", "or", "not", "true", "false",
}
TOKEN_RE = re.compile(
    r"""
    (?P<ws>\s+|//[^\n]*|\#[^\n]*|/\*.*?\*/)
  | (?P<num>0[xX][0-9a-fA-F_]+|0[bB][01_]+|0[oO][0-7_]+|\d[\d_]*)
  | (?P<char>'(?:\\x[0-9a-fA-F]{2}|\\[nrt0\\'"]|[^\\\n'])')
  | (?P<id>[A-Za-z_]\w*)
  | (?P<op>\*\*=|<<=|>>=|\*\*|<<|>>|<=|>=|==|!=|&&|\|\||\+\+|--|\+=|-=|\*=|/=|%=|&=|\|=|\^=|::|[-+*/%&|^~!<>=(){};,?:.])
    """,
    re.S | re.X,
)
CHAR_ESC = {"n": 10, "r": 13, "t": 9, "0": 0, "\\": 92, "'": 39, '"': 34}


class QSError(Exception):
    """A compile error or a trap (the program stops, as the VM would)."""


class _Return(Exception):
    def __init__(self, value):
        self.value = value


class _Break(Exception):
    pass


class _Continue(Exception):
    pass


class _Exit(Exception):
    def __init__(self, code):
        self.code = code


def wrap(x):
    x &= MASK
    return x - (1 << 64) if x >> 63 else x


def tokenize(src, name):
    toks = []
    line = 1
    pos = 0
    while pos < len(src):
        m = TOKEN_RE.match(src, pos)
        if not m:
            raise QSError(f"{name}:{line}: unexpected character {src[pos]!r}")
        kind = m.lastgroup
        text = m.group(kind)
        if kind == "ws":
            pass
        elif kind == "num":
            clean = text.replace("_", "")
            if clean[:2].lower() == "0x":
                value = int(clean[2:], 16)
            elif clean[:2].lower() == "0b":
                value = int(clean[2:], 2)
            elif clean[:2].lower() == "0o":
                value = int(clean[2:], 8)
            else:
                value = int(clean, 10)
            toks.append(("num", wrap(value), line))
        elif kind == "char":
            body = text[1:-1]
            if body.startswith("\\x"):
                value = int(body[2:], 16)
            elif body.startswith("\\"):
                value = CHAR_ESC[body[1]]
            else:
                value = ord(body)
            toks.append(("num", value, line))
        elif kind == "id":
            toks.append(("kw" if text in KEYWORDS else "id", text, line))
        else:
            toks.append(("op", text, line))
        line += text.count("\n")
        pos = m.end()
    toks.append(("eof", "", line))
    return toks


class Parser:
    def __init__(self, toks, name):
        self.t = toks
        self.i = 0
        self.name = name

    def peek(self, k=0):
        return self.t[self.i + k]

    def at(self, kind, value=None, k=0):
        tok = self.peek(k)
        return tok[0] == kind and (value is None or tok[1] == value)

    def op(self, value):
        return self.at("op", value)

    def kw(self, value):
        return self.at("kw", value)

    def take(self):
        tok = self.t[self.i]
        self.i += 1
        return tok

    def expect(self, value):
        tok = self.peek()
        if tok[1] != value or tok[0] not in ("op", "kw"):
            raise QSError(f"{self.name}:{tok[2]}: expected '{value}', found '{tok[1] or 'end of input'}'")
        return self.take()

    def ident(self):
        tok = self.peek()
        if tok[0] != "id":
            raise QSError(f"{self.name}:{tok[2]}: expected a name, found '{tok[1] or 'end of input'}'")
        return self.take()[1]

    def qname(self):
        parts = [self.ident()]
        while self.op("::"):
            self.take()
            parts.append(self.ident())
        return "::".join(parts)

    # ---- declarations ----
    def program(self):
        items = []
        while not self.at("eof"):
            if self.kw("use"):
                self.take()
                names = [self.qname()]
                while self.op(","):
                    self.take()
                    names.append(self.qname())
                self.expect(";")
                items.append(("use", names))
            elif self.kw("fn") or self.kw("sub"):
                self.take()
                name = self.ident()
                self.expect("(")
                params = []
                if not self.op(")"):
                    params.append(self.ident())
                    while self.op(","):
                        self.take()
                        params.append(self.ident())
                self.expect(")")
                items.append(("fn", name, params, self.block()))
            elif self.kw("const"):
                self.take()
                name = self.ident()
                self.expect("=")
                value = self.expr()
                self.expect(";")
                items.append(("const", name, value))
            else:
                items.append(self.statement())
        return items

    # ---- statements ----
    def block(self):
        self.expect("{")
        body = []
        while not self.op("}"):
            if self.at("eof"):
                raise QSError(f"{self.name}: unterminated block")
            body.append(self.statement())
        self.take()
        return ("block", body)

    def modifier(self):
        if self.kw("if") or self.kw("unless"):
            neg = self.take()[1] == "unless"
            return neg, self.expr()
        return None

    def wrap_modifier(self, stmt, mod):
        if mod is None:
            return stmt
        neg, cond = mod
        return ("if", [(cond, stmt)], None, neg)

    def statement(self):
        tok = self.peek()
        if self.op("{"):
            return self.block()
        if self.op(";"):
            self.take()
            return ("block", [])
        if tok[0] == "kw":
            k = tok[1]
            if k in ("let", "my"):
                node = self.let()
                self.expect(";")
                return node
            if k == "if":
                self.take()
                branches = [(self.cond(), self.statement())]
                while self.kw("elsif"):
                    self.take()
                    branches.append((self.cond(), self.statement()))
                else_ = self.statement() if self.kw("else") and self.take() else None
                return ("if", branches, else_, False)
            if k == "unless":
                self.take()
                cond = self.cond()
                body = self.statement()
                else_ = self.statement() if self.kw("else") and self.take() else None
                return ("if", [(cond, body)], else_, True)
            if k == "while":
                self.take()
                cond = self.cond()
                return ("while", cond, self.statement(), False)
            if k == "until":
                self.take()
                cond = self.cond()
                return ("while", cond, self.statement(), True)
            if k == "do":
                self.take()
                body = self.block()
                if self.kw("while"):
                    neg = False
                    self.take()
                elif self.kw("until"):
                    neg = True
                    self.take()
                else:
                    raise QSError(f"{self.name}:{tok[2]}: do block needs while or until")
                cond = self.expr()
                self.expect(";")
                return ("dowhile", body, cond, neg)
            if k == "for":
                self.take()
                self.expect("(")
                init = None
                if not self.op(";"):
                    init = self.let() if (self.kw("let") or self.kw("my")) else self.simple()
                self.expect(";")
                test = None if self.op(";") else self.expr()
                self.expect(";")
                step = None if self.op(")") else self.simple()
                self.expect(")")
                return ("for", init, test, step, self.statement())
            if k == "loop":
                self.take()
                return ("loop", self.block())
            if k in ("break", "last"):
                self.take()
                node = ("break",)
                mod = self.modifier()
                self.expect(";")
                return self.wrap_modifier(node, mod)
            if k in ("continue", "next"):
                self.take()
                node = ("continue",)
                mod = self.modifier()
                self.expect(";")
                return self.wrap_modifier(node, mod)
            if k in ("return", "exit"):
                self.take()
                value = None if (self.op(";") or self.kw("if") or self.kw("unless")) else self.expr()
                mod = self.modifier()
                self.expect(";")
                node = ("return", value) if k == "return" else ("exit", value)
                return self.wrap_modifier(node, mod)
            if k in ("print", "say"):
                self.take()
                values = [self.expr()]
                while self.op(","):
                    self.take()
                    values.append(self.expr())
                mod = self.modifier()
                self.expect(";")
                return self.wrap_modifier(("print", values), mod)
            if k == "assert":
                self.take()
                value = self.expr()
                mod = self.modifier()
                self.expect(";")
                return self.wrap_modifier(("assert", value), mod)
        node = self.simple()
        mod = self.modifier()
        self.expect(";")
        return self.wrap_modifier(node, mod)

    def cond(self):
        self.expect("(")
        value = self.expr()
        self.expect(")")
        return value

    def let(self):
        self.take()
        name = self.ident()
        self.expect("=")
        return ("let", name, self.expr())

    def simple(self):
        if self.at("id") and self.at("op", None, 1):
            nxt = self.peek(1)[1]
            if nxt in ("=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>=", "**="):
                name = self.take()[1]
                op = self.take()[1]
                return ("assign", name, op, self.expr())
            if nxt in ("++", "--"):
                name = self.take()[1]
                op = self.take()[1]
                return ("incdec", name, op)
        return ("expr", self.expr())

    # ---- expressions (C-like precedence; ** binds tightest) ----
    def expr(self):
        cond = self.logic_or()
        if self.op("?"):
            self.take()
            then = self.expr()
            self.expect(":")
            other = self.expr()
            return ("cond", cond, then, other)
        return cond

    BIN_LEVELS = [
        {"||", "or"},
        {"&&", "and"},
        {"|"},
        {"^"},
        {"&"},
        {"==", "!="},
        {"<", "<=", ">", ">="},
        {"<<", ">>"},
        {"+", "-"},
        {"*", "/", "%"},
    ]

    def logic_or(self):
        return self.binary(0)

    def binary(self, level):
        if level == len(self.BIN_LEVELS):
            return self.power()
        left = self.binary(level + 1)
        while self.peek()[0] in ("op", "kw") and self.peek()[1] in self.BIN_LEVELS[level]:
            op = self.take()[1]
            right = self.binary(level + 1)
            left = ("bin", op, left, right)
        return left

    def power(self):
        base = self.unary()
        if self.op("**"):
            self.take()
            return ("bin", "**", base, self.power())
        return base

    def unary(self):
        if self.op("-") or self.op("+") or self.op("!") or self.op("~") or self.kw("not"):
            op = self.take()[1]
            return ("un", op, self.unary())
        return self.primary()

    def primary(self):
        tok = self.peek()
        if tok[0] == "num":
            self.take()
            return ("const_value", tok[1])
        if self.kw("true"):
            self.take()
            return ("const_value", 1)
        if self.kw("false"):
            self.take()
            return ("const_value", 0)
        if self.op("("):
            self.take()
            value = self.expr()
            self.expect(")")
            return value
        if tok[0] == "id":
            name = self.qname()
            if self.op("("):
                self.take()
                args = []
                if not self.op(")"):
                    args.append(self.expr())
                    while self.op(","):
                        self.take()
                        args.append(self.expr())
                self.expect(")")
                return ("call", name, args, tok[2])
            return ("name", name, tok[2])
        raise QSError(f"{self.name}:{tok[2]}: unexpected '{tok[1] or 'end of input'}'")


class Module:
    def __init__(self, name, is_program=False):
        self.name = name
        self.fns = {}
        self.consts = {}
        self.const_values = {}
        self.imports = {}   # unqualified name -> ("fn"|"const", Module, key)
        self.used = set()
        self.program_globals = {} if is_program else None


class Interpreter:
    def __init__(self, lib_dirs, natives, out=None, step_limit=20_000_000):
        self.lib_dirs = lib_dirs
        self.natives = natives          # "carbine::name" -> (arity, fn(args) -> int)
        self.libs = {}                  # lib name -> Module
        self.out = out or sys.stdout
        self.steps = 0
        self.step_limit = step_limit

    # ---- loading ----
    def find_lib(self, name):
        rel = name.replace("::", os.sep) + ".qs"
        for d in self.lib_dirs:
            path = os.path.join(d, rel)
            if os.path.isfile(path):
                return path
        raise QSError(f"no library named {name}")

    def load_lib(self, name):
        if name in self.libs:
            return self.libs[name]
        path = self.find_lib(name)
        with open(path, encoding="utf-8") as f:
            items = Parser(tokenize(f.read(), path), path).program()
        mod = Module(name)
        self.libs[name] = mod          # registered before use: cycles are fine
        self.populate(mod, items, path)
        return mod

    def populate(self, mod, items, path):
        for item in items:
            if item[0] == "use":
                for lib in item[1]:
                    dep = self.load_lib(lib)
                    self.import_public(mod, dep, lib)
            elif item[0] == "fn":
                if item[1] in mod.fns:
                    raise QSError(f"{path}: fn {item[1]} defined twice")
                mod.fns[item[1]] = item
            elif item[0] == "const":
                mod.consts[item[1]] = item
            else:
                if mod.program_globals is None:
                    raise QSError(f"{path}: a library may only contain fn, const and use")
                mod.program_items = getattr(mod, "program_items", []) + [item]

    def import_public(self, into, dep, lib):
        for name in dep.fns:
            if name.startswith("_"):
                continue
            if name in into.imports or name in into.fns:
                raise QSError(f"{into.name}: name {name} imported twice (from {lib})")
            into.imports[name] = ("fn", dep, name)
        for name in dep.consts:
            if name.startswith("_"):
                continue
            if name in into.imports or name in into.consts:
                raise QSError(f"{into.name}: name {name} imported twice (from {lib})")
            into.imports[name] = ("const", dep, name)
        into.used.add(lib)

    # ---- resolution ----
    def resolve_fn(self, mod, name):
        if "::" in name:
            head, _, rest = name.partition("::")
            if head == "carbine":
                return ("native", None, name)
            lib = self.libs.get(name.rpartition("::")[0])
            if lib is None or rest.rpartition("::")[2] not in lib.fns:
                raise QSError(f"unknown function {name}")
            return ("fn", lib, name.rpartition("::")[2])
        if name in mod.fns:
            return ("fn", mod, name)
        if name in mod.imports and mod.imports[name][0] == "fn":
            return mod.imports[name]
        raise QSError(f"unknown function {name}")

    def const_value(self, mod, name):
        if name in mod.const_values:
            return mod.const_values[name]
        if name in mod.consts:
            item = mod.consts[name]
            if name in getattr(mod, "_const_busy", set()):
                raise QSError(f"const {name} depends on itself")
            mod._const_busy = getattr(mod, "_const_busy", set()) | {name}
            value = self.eval(mod, None, item[2])
            mod.const_values[name] = value
            return value
        if name in mod.imports and mod.imports[name][0] == "const":
            _, dep, key = mod.imports[name]
            return self.const_value(dep, key)
        return None

    # ---- execution ----
    def run_program(self, path):
        with open(path, encoding="utf-8") as f:
            items = Parser(tokenize(f.read(), path), path).program()
        mod = Module(os.path.basename(path), is_program=True)
        mod.program_items = []
        for item in items:
            if item[0] == "use":
                for lib in item[1]:
                    self.import_public(mod, self.load_lib(lib), lib)
            elif item[0] == "fn":
                mod.fns[item[1]] = item
            elif item[0] == "const":
                mod.consts[item[1]] = item
            else:
                mod.program_items.append(item)
        frame = {"scopes": [mod.program_globals], "module": mod, "top": True}
        try:
            for item in mod.program_items:
                self.exec(frame, item)
        except _Return as r:
            return r.value
        except _Exit as e:
            return e.code
        return 0

    def call(self, mod, name, args, line):
        kind, owner, key = self.resolve_fn(mod, name)
        if kind == "native":
            if key not in self.natives:
                raise QSError(f"line {line}: unknown native {key}")
            arity, fn = self.natives[key]
            if len(args) != arity:
                raise QSError(f"line {line}: {key} takes {arity} argument(s)")
            self.out.flush()
            try:
                result = fn(args)
            finally:
                self.out.flush()
            return wrap(result)
        item = owner.fns[key]
        _, _, params, body = item
        if len(args) != len(params):
            raise QSError(f"line {line}: {key} takes {len(params)} argument(s), got {len(args)}")
        frame = {"scopes": [dict(zip(params, args))], "module": owner, "top": False}
        try:
            self.exec(frame, body)
        except _Return as r:
            return r.value
        return 0

    def lookup(self, frame, name, line):
        for scope in reversed(frame["scopes"]):
            if name in scope:
                return scope
        value = self.const_value(frame["module"], name)
        if value is not None:
            return {name: value}
        raise QSError(f"line {line}: unknown variable {name}")

    def eval(self, mod, frame, node):
        kind = node[0]
        if kind == "const_value":
            return node[1]
        if kind == "name":
            if frame is None:
                return self.const_value_or_fail(mod, node[1], node[2])
            return self.lookup(frame, node[1], node[2])[node[1]]
        if kind == "call":
            args = [self.eval_in(mod, frame, a) for a in node[2]]
            return self.call(mod, node[1], args, node[3])
        if kind == "un":
            v = self.eval_in(mod, frame, node[2])
            op = node[1]
            if op == "-":
                return wrap(-v)
            if op == "+":
                return v
            if op in ("!", "not"):
                return 0 if v else 1
            return wrap(~v)
        if kind == "cond":
            return self.eval_in(mod, frame, node[2] if self.eval_in(mod, frame, node[1]) else node[3])
        if kind == "bin":
            return self.binop(mod, frame, node)
        raise QSError(f"internal: unknown expression {kind}")

    def const_value_or_fail(self, mod, name, line):
        value = self.const_value(mod, name)
        if value is None:
            raise QSError(f"line {line}: unknown name {name}")
        return value

    def eval_in(self, mod, frame, node):
        return self.eval(mod, frame, node)

    def binop(self, mod, frame, node):
        op = node[1]
        if op in ("&&", "and"):
            return 1 if self.eval_in(mod, frame, node[2]) and self.eval_in(mod, frame, node[3]) else 0
        if op in ("||", "or"):
            return 1 if self.eval_in(mod, frame, node[2]) or self.eval_in(mod, frame, node[3]) else 0
        a = self.eval_in(mod, frame, node[2])
        b = self.eval_in(mod, frame, node[3])
        return self.arith(op, a, b)

    def arith(self, op, a, b):
        if op == "+":
            return wrap(a + b)
        if op == "-":
            return wrap(a - b)
        if op == "*":
            return wrap(a * b)
        if op in ("/", "%"):
            if b == 0:
                raise QSError("division by zero")
            q = abs(a) // abs(b)
            if (a < 0) != (b < 0):
                q = -q
            return wrap(q) if op == "/" else wrap(a - b * q)
        if op == "**":
            if b < 0:
                raise QSError("negative exponent")
            return wrap(a ** b)
        if op == "<<":
            if not 0 <= b < 64:
                raise QSError("shift out of range")
            return wrap(a << b)
        if op == ">>":
            if not 0 <= b < 64:
                raise QSError("shift out of range")
            return wrap(a >> b)
        if op == "&":
            return wrap(a & b)
        if op == "|":
            return wrap(a | b)
        if op == "^":
            return wrap(a ^ b)
        table = {"==": a == b, "!=": a != b, "<": a < b, "<=": a <= b, ">": a > b, ">=": a >= b}
        return 1 if table[op] else 0

    def tick(self):
        self.steps += 1
        if self.steps > self.step_limit:
            raise QSError("step limit exceeded")

    def declare(self, frame, name, value):
        frame["scopes"][-1][name] = value

    def exec(self, frame, node):
        self.tick()
        kind = node[0]
        mod = frame["module"]
        if kind == "block":
            frame["scopes"].append({})
            try:
                for s in node[1]:
                    self.exec(frame, s)
            finally:
                frame["scopes"].pop()
        elif kind == "let":
            self.declare(frame, node[1], self.eval_in(mod, frame, node[2]))
        elif kind == "assign":
            scope = self.lookup(frame, node[1], 0)
            rhs = self.eval_in(mod, frame, node[3])
            if node[2] == "=":
                scope[node[1]] = rhs
            else:
                scope[node[1]] = self.arith(node[2][:-1], scope[node[1]], rhs)
        elif kind == "incdec":
            scope = self.lookup(frame, node[1], 0)
            scope[node[1]] = wrap(scope[node[1]] + (1 if node[2] == "++" else -1))
        elif kind == "expr":
            self.eval_in(mod, frame, node[1])
        elif kind == "if":
            taken = False
            for cond, body in node[1]:
                truth = self.eval_in(mod, frame, cond) != 0
                if node[3]:
                    truth = not truth
                if truth:
                    self.exec(frame, body)
                    taken = True
                    break
            if not taken and node[2] is not None:
                self.exec(frame, node[2])
        elif kind == "while":
            while True:
                truth = self.eval_in(mod, frame, node[1]) != 0
                if node[3]:
                    truth = not truth
                if not truth:
                    break
                try:
                    self.exec(frame, node[2])
                except _Break:
                    break
                except _Continue:
                    continue
        elif kind == "dowhile":
            while True:
                try:
                    self.exec(frame, node[1])
                except _Break:
                    break
                except _Continue:
                    pass
                truth = self.eval_in(mod, frame, node[2]) != 0
                if node[3]:
                    truth = not truth
                if not truth:
                    break
        elif kind == "for":
            frame["scopes"].append({})
            try:
                if node[1] is not None:
                    self.exec(frame, node[1])
                while node[2] is None or self.eval_in(mod, frame, node[2]) != 0:
                    try:
                        self.exec(frame, node[4])
                    except _Break:
                        break
                    except _Continue:
                        pass
                    if node[3] is not None:
                        self.exec(frame, node[3])
            finally:
                frame["scopes"].pop()
        elif kind == "loop":
            while True:
                try:
                    self.exec(frame, node[1])
                except _Break:
                    break
                except _Continue:
                    pass
        elif kind == "break":
            raise _Break()
        elif kind == "continue":
            raise _Continue()
        elif kind == "return":
            value = self.eval_in(mod, frame, node[1]) if node[1] is not None else 0
            raise _Return(value)
        elif kind == "exit":
            value = self.eval_in(mod, frame, node[1]) if node[1] is not None else 0
            raise _Exit(value)
        elif kind == "print":
            for v in node[1]:
                self.out.write(f"{self.eval_in(mod, frame, v)}\n")
        elif kind == "assert":
            if self.eval_in(mod, frame, node[1]) == 0:
                raise QSError("assertion failed")
        else:
            raise QSError(f"internal: unknown statement {kind}")


def run_file(path, lib_dirs, natives, out=None):
    """Runs the program at path. Returns (result, steps). Traps raise QSError."""
    interp = Interpreter(lib_dirs, natives, out=out)
    result = interp.run_program(path)
    return result, interp.steps


def main(argv):
    if len(argv) < 2:
        print("usage: qsi.py LIBDIR[,LIBDIR...] PROGRAM.qs", file=sys.stderr)
        return 2
    dirs = argv[0].split(",")
    try:
        result, _ = run_file(argv[1], dirs, {})
    except QSError as e:
        print(f"qsi: {e}", file=sys.stderr)
        return 1
    print(f"result {result}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
