#!/usr/bin/env python3
"""Execute specs, validate backend references, and check useful backend facts.

The external DomSymAsCC DSL is supplied with --dsl-dir. llvm-mc supplies an
independent encoding check for new targets that support assembly parsing.
"""
import argparse
import ast
import json
from pathlib import Path
import re
import subprocess

from extract_backend import GROUPS, refs


def load_document(path, dsl_dir):
    root = path.parent.resolve()
    loader = 'package.path = arg[1] .. "/?.lua;" .. arg[2] .. "/?.lua;" .. package.path; dofile(arg[3])'
    result = subprocess.run(["lua", "-", str(root), str(dsl_dir), str(path.resolve())],
                            input=loader, text=True, capture_output=True, check=True)
    return json.loads(result.stdout)


def validate(doc, arch):
    backend = doc["backend"]
    assert backend["schema"] == 1 and backend["arch"] == arch
    records = {}
    for group in (*GROUPS, "instructions", "dependencies"):
        records.update(backend.get(group, {}))
    dangling = set(refs(records)) - records.keys()
    assert not dangling, (arch, "unresolved references", sorted(dangling))
    for name, item in backend["instructions"].items():
        for register in refs([item.get("Defs"), item.get("Uses")]):
            assert "Register" in records[register]["classes"], (name, register)
        for field in ("mayLoad", "mayStore", "hasSideEffects"):
            assert item[field] in (None, 0, 1), (name, field)
    for item in backend.get("compiler_policy", {}).get("allocation", {}).get("conditional_reserved", []):
        assert all(r in records for r in item["registers"])
    for register in backend.get("compiler_policy", {}).get("allocation", {}).get("reserved", []):
        assert register in records, register
    names = {r["name"] for r in doc["registers"]}
    assert all(not r.get("parent") or r["parent"] in names for r in doc["registers"])
    assert all(f["encoder"] in {e["name"] for e in doc["encoders"]} for f in doc["forms"])
    return backend


def evaluate(expression, variables):
    # Evaluate only the arithmetic subset used by the new concrete encoders.
    node = ast.parse(expression, mode="eval")
    allowed = (ast.Expression, ast.Constant, ast.Name, ast.Load, ast.BinOp,
               ast.Add, ast.Sub, ast.Mult, ast.Mod, ast.Div, ast.Compare,
               ast.GtE, ast.LtE, ast.Gt, ast.Lt, ast.Eq)
    assert all(isinstance(n, allowed) for n in ast.walk(node))
    return eval(compile(node, "<encoding>", "eval"), {"__builtins__": {}}, variables)


def encode(source, variables):
    result = bytearray()
    for statement in source.split(";"):
        if not statement.strip():
            continue
        match = re.fullmatch(r"\s*(require|emit16le|emit32be)\((.*)\)\s*", statement)
        assert match, statement
        value = evaluate(match[2], variables)
        if match[1] == "require":
            assert value, (source, variables)
        else:
            width, endian = (2, "little") if match[1] == "emit16le" else (4, "big")
            result.extend(int(value).to_bytes(width, endian))
    return list(result)


def check_encodings(documents):
    examples = {
        "msp430": [("mov_rr", "mov r5, r6", {"rs": 5, "rd": 6}),
                   ("add_rr", "add r7, r8", {"rs": 7, "rd": 8}),
                   ("nop", "nop", {}), ("ret", "ret", {})],
        "lanai": [("add_i_lo", "add %r3, 42, %r9", {"rs": 3, "rd": 9, "imm": 42}),
                  ("add_i_lo", "add %r31, 65535, %r30", {"rs": 31, "rd": 30, "imm": 65535}),
                  ("nop", "nop", {})],
    }
    for arch, cases in examples.items():
        if arch not in documents:
            continue
        encoders = {e["name"]: e["source"] for e in documents[arch]["encoders"]}
        for encoder, assembly, operands in cases:
            output = subprocess.check_output(["llvm-mc", "--triple=" + arch, "--show-encoding"],
                                             input=assembly + "\n", text=True)
            byte_text = re.search(r"encoding: \[([^]]+)\]", output)[1]
            expected = [int(b.strip(), 16) for b in byte_text.split(",")]
            assert encode(encoders[encoder], operands) == expected, (arch, assembly)
    # XCore has no LLVM assembly parser. Check every exposed short encoder
    # against the resolved TableGen bit vector at register/immediate boundaries.
    if "xcore" in documents:
        doc = documents["xcore"]
        encoders = {e["name"]: e["source"] for e in doc["encoders"]}
        mapping = {"ldw_ru6": "LDWSP_ru6", "stw_ru6": "STWSP_ru6",
                   "ldc_ru6": "LDC_ru6", "entsp_u6": "ENTSP_u6", "retsp_u6": "RETSP_u6"}
        for encoder, record in mapping.items():
            for a, b in ((0, 0), (3, 42), (11, 63)):
                operands = {"a": a, "b": b}
                bits = doc["backend"]["instructions"][record]["Inst"][:16]
                assert None not in bits
                word = sum((bit if isinstance(bit, int) else
                            (operands[bit["var"]] >> bit["index"]) & 1) << i
                           for i, bit in enumerate(bits))
                assert encode(encoders[encoder], operands) == list(word.to_bytes(2, "little"))


def check_compiler_facts(documents):
    if "msp430" in documents:
        b = documents["msp430"]["backend"]
        add = b["instructions"]["ADD16rr"]
        assert "$src2 = $rd" in add["Constraints"]
        assert "SR" in set(refs(add["Defs"]))
        assert b["compiler_policy"]["allocation"]["frame_pointer"] == "R4"
        assert b["sleigh"]["constructors"]
    if "lanai" in documents:
        b = documents["lanai"]["backend"]
        model = b["instruction_scheduling"]["LanaiSchedModel"]
        assert model["IssueWidth"] == 1 and model["LoadLatency"] == 2
        assert model["CompleteModel"] == 0
    if "xcore" in documents:
        assert documents["xcore"]["backend"]["compiler_policy"]["allocation"]["reserved"] == ["CP", "DP", "SP", "LR"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dsl-dir", type=Path, required=True)
    parser.add_argument("specs", type=Path, nargs="*")
    args = parser.parse_args()
    documents = {}
    failures = []
    for path in args.specs or sorted(Path(__file__).resolve().parents[1].glob("*.dsymspec")):
        try:
            doc = load_document(path, args.dsl_dir.resolve())
            validate(doc, path.stem)
            documents[path.stem] = doc
            print(path.name, "OK")
        except (AssertionError, KeyError, ValueError, subprocess.CalledProcessError) as error:
            detail = error.stderr if isinstance(error, subprocess.CalledProcessError) else str(error)
            failures.append((str(path), detail))
    for path, detail in failures:
        print(path, "FAILED:", detail)
    if failures:
        raise SystemExit(1)
    check_encodings(documents)
    check_compiler_facts(documents)
    print("Backend integrity, compiler facts and encoding checks passed.")


if __name__ == "__main__":
    main()
