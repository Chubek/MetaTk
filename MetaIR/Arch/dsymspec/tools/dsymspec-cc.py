#!/usr/bin/env python3
"""Compile .dsymspec.yaml files to binary bytecode (.dsymc), and decode them.

The bytecode is a flat, checksummed stream. Each DSL verb has an opcode, and each
encoder body compiles to one opcode per pseudo-code statement. Expressions are
stored as interned text (they are not compiled further).

Container layout (all integers little-endian):

    header   20 bytes  magic "DSYC", u16 version (1), u16 flags (0),
                       u32 string count, u32 string-table bytes, u32 code bytes
    strings  string count entries of: uvarint length, UTF-8 bytes
    code     opcode stream, ends with OP_HALT
    trailer  32 bytes  SHA-256 of everything before it

Integers in code are unsigned LEB128 (uvarint) unless noted. Strings are
referred to by string-table index. A value is a tagged tree:

    0 NULL   1 FALSE   2 TRUE   3 INT (zigzag svarint)   4 STR (index)
    5 LIST   (count, values...)   6 MAP (count, then key-index + value pairs)

Opcodes:
    0x00 HALT                      0x01 SOURCE_SHA256 (32 raw bytes)
    0x10..0x19 TARGET .. DEBUG     one record each; operand is a value (MAP)
    0x20 ENCODER  (MAP of name, max_bytes, max_relocs ...), then statements,
    0x21 END_ENCODER
    0x30..0x3D statements: REQUIRE, EMIT8, EMIT16LE, EMIT16BE, EMIT32LE,
                EMIT32BE, EMIT64LE, EMIT64BE, EMIT_IMM, EMIT_DISP, EMIT_MODRM,
                EMIT_SIB, EMIT_RELOCATION, FAIL; each operand is a string index
    0x40 BACKEND  (uvarint length, zlib-compressed backend JSON)

Usage:
    python3 tools/dsymspec-cc.py                         # all specs -> build/bytecode/<arch>.dsymc
    python3 tools/dsymspec-cc.py -o out/ x86.dsymspec.yaml
    python3 tools/dsymspec-cc.py --decode build/bytecode/lanai.dsymc   # bytecode -> JSON on stdout

Compilation decodes its own output and fails unless the result equals the JSON
document from dsymspec2json.py, so a bad encoding cannot be written silently.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dsymspec_core as core  # noqa: E402

MAGIC = b"DSYC"
VERSION = 1
HEADER = struct.Struct("<4sHHIII")
SHA_LEN = 32

OP_HALT = 0x00
OP_SOURCE_SHA256 = 0x01
RECORD_OP = {"target": 0x10, "feature": 0x11, "register": 0x12, "mnemonic": 0x13,
             "form": 0x14, "syntax": 0x15, "section": 0x16, "linkage": 0x17,
             "reloc": 0x18, "debug": 0x19}
OP_ENCODER = 0x20
OP_END_ENCODER = 0x21
STMT_OP = {"REQUIRE": 0x30, "EMIT8": 0x31, "EMIT16LE": 0x32, "EMIT16BE": 0x33,
           "EMIT32LE": 0x34, "EMIT32BE": 0x35, "EMIT64LE": 0x36, "EMIT64BE": 0x37,
           "EMIT_IMM": 0x38, "EMIT_DISP": 0x39, "EMIT_MODRM": 0x3A, "EMIT_SIB": 0x3B,
           "EMIT_RELOCATION": 0x3C, "FAIL": 0x3D}
OP_BACKEND = 0x40

T_NULL, T_FALSE, T_TRUE, T_INT, T_STR, T_LIST, T_MAP = range(7)

RECORD_BY_OP = {code: kind for kind, code in RECORD_OP.items()}
STMT_BY_OP = {code: name for name, code in STMT_OP.items()}


class BytecodeError(Exception):
    pass


# ----------------------------------------------------------------- writing
class _Writer:
    def __init__(self) -> None:
        self._ids: dict[str, int] = {}
        self.table: list[str] = []
        self.code = bytearray()

    def sid(self, s: str) -> int:
        if s not in self._ids:
            self._ids[s] = len(self.table)
            self.table.append(s)
        return self._ids[s]

    def uvar(self, n: int) -> None:
        if n < 0:
            raise ValueError("uvarint must be non-negative")
        while True:
            b = n & 0x7F
            n >>= 7
            if n:
                self.code.append(b | 0x80)
            else:
                self.code.append(b)
                return

    def svar(self, n: int) -> None:
        self.uvar(2 * n if n >= 0 else -2 * n - 1)

    def op(self, code: int) -> None:
        self.code.append(code)

    def value(self, v) -> None:
        if v is None:
            self.code.append(T_NULL)
        elif v is False:
            self.code.append(T_FALSE)
        elif v is True:
            self.code.append(T_TRUE)
        elif isinstance(v, int):
            self.code.append(T_INT)
            self.svar(v)
        elif isinstance(v, str):
            self.code.append(T_STR)
            self.uvar(self.sid(v))
        elif isinstance(v, list):
            self.code.append(T_LIST)
            self.uvar(len(v))
            for x in v:
                self.value(x)
        elif isinstance(v, dict):
            self.code.append(T_MAP)
            self.uvar(len(v))
            for k, x in v.items():
                if not isinstance(k, str):
                    raise TypeError("map keys must be strings")
                self.uvar(self.sid(k))
                self.value(x)
        else:
            raise TypeError("cannot encode %r" % (type(v),))


def compile_spec(spec: dict, spec_dir: Path, source: bytes) -> bytes:
    w = _Writer()
    w.op(OP_SOURCE_SHA256)
    w.code += hashlib.sha256(source).digest()
    for key, kind in core.SECTIONS:
        for rec in spec.get(key) or []:
            if kind == "encoder":
                stmts = core.parse_body(rec["body"], where="encoder %r" % rec.get("name"))
                meta = {k: v for k, v in rec.items() if k != "body"}
                w.op(OP_ENCODER)
                w.value(meta)
                for st in stmts:
                    w.op(STMT_OP[st.op])
                    for arg in st.args:
                        w.uvar(w.sid(arg))
                w.op(OP_END_ENCODER)
            else:
                w.op(RECORD_OP[kind])
                w.value(core.record_to_json(kind, rec))
    if spec.get("backend"):
        raw = (spec_dir / spec["backend"]["file"]).read_bytes()
        packed = zlib.compress(raw, 9)
        w.op(OP_BACKEND)
        w.uvar(len(packed))
        w.code += packed
    w.op(OP_HALT)

    table = bytearray()
    for s in w.table:
        raw = s.encode("utf-8")
        n = len(raw)
        while True:
            b = n & 0x7F
            n >>= 7
            if n:
                table.append(b | 0x80)
            else:
                table.append(b)
                break
        table += raw
    header = HEADER.pack(MAGIC, VERSION, 0, len(w.table), len(table), len(w.code))
    body = header + bytes(table) + bytes(w.code)
    return body + hashlib.sha256(body).digest()


# ----------------------------------------------------------------- reading
class _Reader:
    def __init__(self, data: bytes, pos: int, end: int, strings: list[str]) -> None:
        self.data, self.pos, self.end, self.strings = data, pos, end, strings

    def _need(self, n: int) -> None:
        if self.pos + n > self.end:
            raise BytecodeError("truncated bytecode at offset %d" % self.pos)

    def u8(self) -> int:
        self._need(1)
        b = self.data[self.pos]
        self.pos += 1
        return b

    def uvar(self) -> int:
        shift = result = 0
        while True:
            b = self.u8()
            result |= (b & 0x7F) << shift
            if not b & 0x80:
                return result
            shift += 7
            if shift > 63:
                raise BytecodeError("varint too long at offset %d" % self.pos)

    def svar(self) -> int:
        z = self.uvar()
        return z >> 1 if z % 2 == 0 else -((z + 1) >> 1)

    def take(self, n: int) -> bytes:
        self._need(n)
        out = self.data[self.pos:self.pos + n]
        self.pos += n
        return out

    def sval(self) -> str:
        i = self.uvar()
        if i >= len(self.strings):
            raise BytecodeError("string index %d out of range" % i)
        return self.strings[i]

    def value(self):
        tag = self.u8()
        if tag == T_NULL:
            return None
        if tag == T_FALSE:
            return False
        if tag == T_TRUE:
            return True
        if tag == T_INT:
            return self.svar()
        if tag == T_STR:
            return self.sval()
        if tag == T_LIST:
            return [self.value() for _ in range(self.uvar())]
        if tag == T_MAP:
            out = {}
            for _ in range(self.uvar()):
                k = self.sval()
                out[k] = self.value()
            return out
        raise BytecodeError("bad value tag %d at offset %d" % (tag, self.pos - 1))


def decode(data: bytes) -> tuple[dict, str]:
    """Bytecode -> (JSON document as dict, hex SHA-256 of the source YAML)."""
    if len(data) < HEADER.size + SHA_LEN:
        raise BytecodeError("file too short")
    magic, version, flags, nstr, strbytes, codebytes = HEADER.unpack_from(data, 0)
    if magic != MAGIC:
        raise BytecodeError("bad magic %r" % (magic,))
    if version != VERSION:
        raise BytecodeError("unsupported bytecode version %d" % version)
    if flags != 0:
        raise BytecodeError("unknown flags %#x" % flags)
    if len(data) != HEADER.size + strbytes + codebytes + SHA_LEN:
        raise BytecodeError("length mismatch: header disagrees with file size")
    if hashlib.sha256(data[:-SHA_LEN]).digest() != data[-SHA_LEN:]:
        raise BytecodeError("checksum mismatch")

    table_start = HEADER.size
    table_end = table_start + strbytes
    tr = _Reader(data, table_start, table_end, [])
    strings: list[str] = []
    for _ in range(nstr):
        n = tr.uvar()
        strings.append(tr.take(n).decode("utf-8"))
    if tr.pos != table_end:
        raise BytecodeError("string table size mismatch")

    r = _Reader(data, table_end, table_end + codebytes, strings)
    sections: dict[str, list] = {key: [] for key, _ in core.SECTIONS}
    key_of_kind = {kind: key for key, kind in core.SECTIONS}
    backend = None
    source_sha = None
    while True:
        op = r.u8()
        if op == OP_HALT:
            break
        if op == OP_SOURCE_SHA256:
            source_sha = r.take(SHA_LEN).hex()
        elif op in RECORD_BY_OP:
            kind = RECORD_BY_OP[op]
            rec = r.value()
            if not isinstance(rec, dict):
                raise BytecodeError("%s record is not a map" % kind)
            sections[key_of_kind[kind]].append(rec)
        elif op == OP_ENCODER:
            meta = r.value()
            stmts = []
            while True:
                sop = r.u8()
                if sop == OP_END_ENCODER:
                    break
                if sop not in STMT_BY_OP:
                    raise BytecodeError("bad statement opcode %#x" % sop)
                name = STMT_BY_OP[sop]
                arity = core.STATEMENTS[name][1]
                args = tuple(r.sval() for _ in range(arity))
                stmts.append(core.Statement(name, args, 0))
            rec = {"name": meta["name"], "source": core.to_algol(stmts)}
            rec.update({k: v for k, v in meta.items() if k != "name"})
            sections["encoders"].append(rec)
        elif op == OP_BACKEND:
            n = r.uvar()
            backend = json.loads(zlib.decompress(r.take(n)).decode("utf-8"))
        else:
            raise BytecodeError("unknown opcode %#x at offset %d" % (op, r.pos - 1))
    if r.pos != r.end:
        raise BytecodeError("trailing bytes after HALT")

    doc: dict = {"schema": core.SCHEMA_VERSION}
    for key, _ in core.SECTIONS:
        doc[key] = sections[key]
    if backend is not None:
        doc["backend"] = backend
    return doc, source_sha


# ----------------------------------------------------------------- CLI
def compile_file(path: Path, out_dir: Path) -> Path:
    source = path.read_bytes()
    spec = core.require_valid(path)
    data = compile_spec(spec, path.parent, source)

    # Self-check: the bytecode must decode to exactly the JSON document.
    doc, sha = decode(data)
    want = core.build_document(spec, path.parent, with_backend=True)
    if json.dumps(doc, ensure_ascii=False) != json.dumps(want, ensure_ascii=False):
        raise BytecodeError("%s: round-trip mismatch; not written" % path)
    if sha != hashlib.sha256(source).hexdigest():
        raise BytecodeError("%s: source hash mismatch" % path)

    out_dir.mkdir(parents=True, exist_ok=True)
    target = out_dir / (core.arch_of(spec) + ".dsymc")
    target.write_bytes(data)
    return target


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("files", nargs="*", type=Path, help="spec files (default: all in repo root)")
    ap.add_argument("-o", "--out-dir", type=Path, default=core.REPO_ROOT / "build" / "bytecode",
                    help="directory for <arch>.dsymc files (default: build/bytecode)")
    ap.add_argument("--decode", type=Path, metavar="FILE",
                    help="decode a .dsymc file and print its JSON document")
    ap.add_argument("--compact", action="store_true", help="with --decode: no indentation")
    args = ap.parse_args(argv)

    if args.decode:
        try:
            doc, sha = decode(args.decode.read_bytes())
        except (BytecodeError, UnicodeDecodeError, OSError) as exc:
            print("%s: %s" % (args.decode, exc), file=sys.stderr)
            return 1
        indent = None if args.compact else 2
        sys.stdout.write(json.dumps(doc, ensure_ascii=False, indent=indent) + "\n")
        print("source sha256 %s" % sha, file=sys.stderr)
        return 0

    files = args.files or core.discover_specs()
    if not files:
        print("no .dsymspec.yaml files found", file=sys.stderr)
        return 2
    status = 0
    for path in files:
        try:
            target = compile_file(path, args.out_dir)
        except (core.SpecError, BytecodeError) as exc:
            print(exc, file=sys.stderr)
            status = 1
            continue
        print("wrote %s (%d bytes)" % (target, target.stat().st_size))
    return status


if __name__ == "__main__":
    sys.exit(main())
