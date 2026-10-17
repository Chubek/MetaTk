"""Shared logic for the dsymspec tools.

A .dsymspec.yaml file is one architecture specification. This module loads it,
checks it, parses its imperative pseudo-code encoder bodies, and builds the
JSON document described in README.md. The other tools are thin front ends:

    validate.py      schema + semantic checks
    dsymspec2json.py YAML -> JSON document
    dsymspec2lua.py  YAML -> Lua data module
    dsymspec-cc.py   YAML -> binary bytecode (and back, for checking)
"""
from __future__ import annotations

import json
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import yaml

SCHEMA_VERSION = 1
REPO_ROOT = Path(__file__).resolve().parent.parent
SCHEMA_PATH = REPO_ROOT / "schema" / "dsymspec.schema.json"

# Top-level YAML key -> record kind. The order is the order of the JSON document.
SECTIONS: list[tuple[str, str]] = [
    ("targets", "target"),
    ("features", "feature"),
    ("registers", "register"),
    ("mnemonics", "mnemonic"),
    ("encoders", "encoder"),
    ("forms", "form"),
    ("syntaxes", "syntax"),
    ("sections", "section"),
    ("linkages", "linkage"),
    ("relocs", "reloc"),
    ("debug", "debug"),
]

# Pseudo-code statement -> (Algol-like name used by domsymas, argument count).
# The Algol-like names are what the JSON `source` field and the Lua output carry.
STATEMENTS: dict[str, tuple[str, int]] = {
    "REQUIRE": ("require", 1),
    "EMIT8": ("emit8", 1),
    "EMIT16LE": ("emit16le", 1),
    "EMIT16BE": ("emit16be", 1),
    "EMIT32LE": ("emit32le", 1),
    "EMIT32BE": ("emit32be", 1),
    "EMIT64LE": ("emit64le", 1),
    "EMIT64BE": ("emit64be", 1),
    "EMIT_IMM": ("emit_imm", 2),
    "EMIT_DISP": ("emit_disp", 2),
    "EMIT_MODRM": ("emit_modrm", 2),
    "EMIT_SIB": ("emit_sib", 3),
    "EMIT_RELOCATION": ("emit_relocation", 2),
    "FAIL": ("fail", 1),
}

_LINE = re.compile(r"^([A-Z][A-Z0-9_]*)(?:[ \t]+(.+))?$")
_IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
_STRING = re.compile(r'^"(?:[^"\\\n]|\\.)*"$')
_TOKEN = re.compile(r"\s*(0[xX][0-9A-Fa-f]+|[0-9]+|[A-Za-z_][A-Za-z0-9_]*|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^~!<>()])")
_BINARY = {"+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>", "<", ">", "<=", ">=",
           "==", "!=", "&&", "||"}
_UNARY = {"+", "-", "~", "!"}
_ATOM = re.compile(r"^(0[xX][0-9A-Fa-f]+|[0-9]+|[A-Za-z_][A-Za-z0-9_]*)$")


def check_expression(text: str) -> None:
    """Raise ValueError unless text is one well-formed infix expression.

    Grammar: expr := unary (binop unary)* ; unary := unop* primary ;
             primary := number | identifier | '(' expr ')'.
    """
    toks, pos = [], 0
    text = text.strip()
    while pos < len(text):
        m = _TOKEN.match(text, pos)
        if not m:
            raise ValueError("unsupported character in %r" % text)
        toks.append(m.group(1))
        pos = m.end()
    i = 0

    def peek():
        return toks[i] if i < len(toks) else None

    def take():
        nonlocal i
        t = peek()
        i += 1
        return t

    def primary():
        t = take()
        if t is None:
            raise ValueError("expression ends too early in %r" % text)
        if t == "(":
            expr()
            if take() != ")":
                raise ValueError("missing ')' in %r" % text)
        elif not _ATOM.match(t):
            raise ValueError("unexpected %r in %r" % (t, text))

    def unary():
        while peek() in _UNARY:
            take()
        primary()

    def expr():
        unary()
        while peek() in _BINARY:
            take()
            unary()

    if not toks:
        raise ValueError("empty expression")
    expr()
    if i != len(toks):
        raise ValueError("unexpected %r in %r" % (peek(), text))


class SpecError(Exception):
    """A spec could not be loaded or is not valid. Message names file and location."""


# --------------------------------------------------------------------- YAML load
class _UniqueKeyLoader(yaml.SafeLoader):
    """SafeLoader that rejects duplicate mapping keys instead of keeping the last."""


def _construct_unique_mapping(loader, node, deep=False):
    seen = set()
    for key_node, _ in node.value:
        key = loader.construct_object(key_node, deep=True)
        if key in seen:
            raise yaml.constructor.ConstructorError(
                "while constructing a mapping", node.start_mark,
                "duplicate key %r" % (key,), key_node.start_mark)
        seen.add(key)
    return yaml.SafeLoader.construct_mapping(loader, node, deep)


_UniqueKeyLoader.add_constructor(
    yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, _construct_unique_mapping)


def load_spec(path: str | os.PathLike) -> dict[str, Any]:
    path = Path(path)
    try:
        with open(path, encoding="utf-8") as f:
            data = yaml.load(f, Loader=_UniqueKeyLoader)
    except yaml.YAMLError as exc:
        raise SpecError("%s: YAML error: %s" % (path, exc)) from None
    except OSError as exc:
        raise SpecError("%s: %s" % (path, exc.strerror)) from None
    if not isinstance(data, dict):
        raise SpecError("%s: top level must be a mapping" % path)
    return data


def arch_of(spec: dict[str, Any]) -> str:
    return spec["targets"][0]["arch"]


# ------------------------------------------------------------ pseudo-code bodies
@dataclass(frozen=True)
class Statement:
    op: str
    args: tuple[str, ...]
    line: int


def _split_top(text: str) -> list[str]:
    """Split on commas that are not inside parentheses."""
    parts, depth, cur = [], 0, []
    for ch in text:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth < 0:
                raise ValueError("unbalanced ')'")
        if ch == "," and depth == 0:
            parts.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    if depth != 0:
        raise ValueError("unbalanced '('")
    parts.append("".join(cur).strip())
    return parts


def parse_body(text: str, where: str = "body") -> list[Statement]:
    """Parse imperative pseudo-code: one statement per line, no blank lines."""
    if not isinstance(text, str):
        raise SpecError("%s: body must be a string" % where)
    lines = text[:-1].split("\n") if text.endswith("\n") else text.split("\n")
    stmts: list[Statement] = []
    for n, raw in enumerate(lines, 1):
        loc = "%s line %d" % (where, n)
        line = raw.strip()
        if not line:
            raise SpecError("%s: blank line" % loc)
        m = _LINE.match(line)
        if not m:
            raise SpecError("%s: expected STATEMENT in upper case, got %r" % (loc, line))
        op, rest = m.group(1), m.group(2) or ""
        if op not in STATEMENTS:
            raise SpecError("%s: unknown statement %s" % (loc, op))
        arity = STATEMENTS[op][1]
        if op == "FAIL":
            if not _STRING.match(rest):
                raise SpecError('%s: FAIL takes one "string" argument' % loc)
            args = (rest,)
        else:
            try:
                args = tuple(_split_top(rest)) if rest else ()
            except ValueError as exc:
                raise SpecError("%s: %s" % (loc, exc)) from None
            if len(args) != arity:
                raise SpecError("%s: %s takes %d argument(s), got %d"
                                % (loc, op, arity, len(args)))
            for a in args:
                if not a:
                    raise SpecError("%s: empty argument" % loc)
                try:
                    check_expression(a)
                except ValueError as exc:
                    raise SpecError("%s: %s" % (loc, exc)) from None
            if op == "EMIT_RELOCATION" and not _IDENT.match(args[1]):
                raise SpecError("%s: relocation type must be an identifier" % loc)
        stmts.append(Statement(op, args, n))
    return stmts


def to_algol(stmts: list[Statement]) -> str:
    """Render statements as the Algol-like encoder source used by domsymas.

    Output is `stmt; stmt;` with single spaces, which is the form the original
    .dsymspec files used, so the rendering is exact for those files.
    """
    parts = []
    for s in stmts:
        fn = STATEMENTS[s.op][0]
        arg_text = s.args[0] if s.op == "FAIL" else ", ".join(s.args)
        parts.append("%s(%s);" % (fn, arg_text))
    return " ".join(parts)


# ------------------------------------------------------------------- semantics
def _power_of_two(n: Any) -> bool:
    return isinstance(n, int) and n > 0 and (n & (n - 1)) == 0


def check_semantics(spec: dict[str, Any], spec_dir: Path | None = None) -> list[str]:
    """Referential-integrity and value checks the schema cannot express.

    Mirrors what the Lua DSL's emit() enforces (unknown mnemonic/encoder/
    feature/syntax names are rejected) plus body grammar and backend linkage.
    Returns human-readable error strings; an empty list means valid.
    """
    errs: list[str] = []
    if spec.get("schema") != SCHEMA_VERSION:
        errs.append("schema: must be %d" % SCHEMA_VERSION)

    def lists(key):
        return spec.get(key) or []

    targets = lists("targets")
    if not targets:
        errs.append("targets: at least one target is required")
    arch = targets[0].get("arch") if targets else None
    for i, t in enumerate(targets):
        if not _power_of_two(t.get("align")):
            errs.append("targets[%d].align: must be a power of two" % i)

    names: dict[str, set[str]] = {}
    for key, kind in SECTIONS:
        if kind in ("target", "form", "debug"):
            continue
        seen: set[str] = set()
        for i, rec in enumerate(lists(key)):
            name = rec.get("name")
            if name in seen:
                errs.append("%s[%d].name: duplicate %r" % (key, i, name))
            seen.add(name)
        names[kind] = seen
    feats = names.get("feature", set())
    syns = names.get("syntax", set())
    regs = names.get("register", set())
    encs = names.get("encoder", set())
    mnems = names.get("mnemonic", set())

    for i, t in enumerate(targets):
        for f in t.get("features", []) or []:
            if f not in feats:
                errs.append("targets[%d].features: unknown feature %r" % (i, f))

    for i, rec in enumerate(lists("sections")):
        if not _power_of_two(rec.get("align")):
            errs.append("sections[%d].align: must be a power of two" % i)

    for i, rec in enumerate(lists("registers")):
        if "parent" in rec and rec["parent"] not in regs:
            errs.append("registers[%d].parent: unknown register %r" % (i, rec["parent"]))

    for i, rec in enumerate(lists("encoders")):
        try:
            parse_body(rec.get("body", ""), where="encoders[%d].body" % i)
        except SpecError as exc:
            errs.append(str(exc))

    for i, rec in enumerate(lists("forms")):
        if rec.get("mnemonic") not in mnems:
            errs.append("forms[%d].mnemonic: unknown mnemonic %r" % (i, rec.get("mnemonic")))
        if rec.get("encoder") not in encs:
            errs.append("forms[%d].encoder: unknown encoder %r" % (i, rec.get("encoder")))
        for k in ("required", "forbidden"):
            for f in rec.get(k, []) or []:
                if f not in feats:
                    errs.append("forms[%d].%s: unknown feature %r" % (i, k, f))
        for s in rec.get("syntaxes", []) or []:
            if s not in syns:
                errs.append("forms[%d].syntaxes: unknown syntax %r" % (i, s))

    backend = spec.get("backend")
    if backend is not None:
        ref = backend.get("file")
        if spec_dir is not None and ref:
            path = Path(spec_dir) / ref
            if not path.is_file():
                errs.append("backend.file: %s does not exist" % ref)
            else:
                try:
                    doc = json.loads(path.read_text(encoding="utf-8"))
                except (ValueError, OSError) as exc:
                    errs.append("backend.file: %s is not valid JSON (%s)" % (ref, exc))
                else:
                    if doc.get("schema") != SCHEMA_VERSION:
                        errs.append("backend.file: %s has schema %r, expected %d"
                                    % (ref, doc.get("schema"), SCHEMA_VERSION))
                    if arch is not None and doc.get("arch") != arch:
                        errs.append("backend.file: %s describes arch %r, spec is %r"
                                    % (ref, doc.get("arch"), arch))
    return errs


def schema_errors(spec: dict[str, Any]) -> list[str]:
    """JSON Schema validation of the loaded YAML data."""
    import jsonschema  # imported lazily so the other helpers work without it

    with open(SCHEMA_PATH, encoding="utf-8") as f:
        schema = json.load(f)
    validator = jsonschema.Draft202012Validator(schema)
    out = []
    for err in sorted(validator.iter_errors(spec), key=lambda e: list(e.absolute_path)):
        where = "/".join(str(p) for p in err.absolute_path) or "<root>"
        out.append("%s: %s" % (where, err.message))
    return out


def validate_spec(path: str | os.PathLike) -> tuple[dict[str, Any], list[str]]:
    """Load and fully validate one spec. Returns (spec, errors)."""
    spec = load_spec(path)
    errors = schema_errors(spec)
    if not errors:  # semantic checks assume the structure the schema guarantees
        errors = check_semantics(spec, Path(path).parent)
    return spec, errors


def require_valid(path: str | os.PathLike) -> dict[str, Any]:
    spec, errors = validate_spec(path)
    if errors:
        raise SpecError("%s: %d error(s):\n  %s" % (path, len(errors), "\n  ".join(errors)))
    return spec


# -------------------------------------------------------------- document build
def record_to_json(kind: str, rec: dict[str, Any]) -> dict[str, Any]:
    if kind == "encoder":
        body = parse_body(rec["body"], where="encoder %r" % rec.get("name"))
        out = {"name": rec["name"], "source": to_algol(body)}
        out.update({k: v for k, v in rec.items() if k not in ("name", "body")})
        return out
    if kind == "form":
        ops = rec.get("operands", [])
        out = {"mnemonic": rec["mnemonic"],
               "ops": rec.get("ops", len(ops))}
        out.update({k: v for k, v in rec.items() if k not in ("mnemonic", "ops")})
        return out
    return dict(rec)


def build_document(spec: dict[str, Any], spec_dir: Path | None,
                   with_backend: bool = True) -> dict[str, Any]:
    """The JSON document of README.md. Assumes the spec has been validated."""
    doc: dict[str, Any] = {"schema": SCHEMA_VERSION}
    for key, kind in SECTIONS:
        doc[key] = [record_to_json(kind, rec) for rec in spec.get(key) or []]
    if with_backend and spec.get("backend"):
        ref = spec["backend"]["file"]
        base = Path(spec_dir) if spec_dir is not None else REPO_ROOT
        doc["backend"] = json.loads((base / ref).read_text(encoding="utf-8"))
    return doc


def discover_specs(directory: Path | None = None) -> list[Path]:
    directory = Path(directory) if directory else REPO_ROOT
    return sorted(directory.glob("*.dsymspec.yaml"))
