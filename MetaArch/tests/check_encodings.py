#!/usr/bin/env python3
"""Recheck the TSV fixtures using an independent llvm-mc (not required by CTest)."""
import argparse
import pathlib
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llvm-mc", default="llvm-mc")
    args = parser.parse_args()
    triples = {"ppc32": "ppc32", "ppc64": "ppc64le",
               "mips32": "mipsel", "mips64": "mips64el", "riscv64": "riscv64"}
    count = 0
    table = pathlib.Path(__file__).with_name("encodings.tsv")
    for lineno, line in enumerate(table.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        arch, enc, assignments, expected = line.split("\t")
        operands = dict(item.split("=", 1) for item in assignments.split())
        if arch.startswith("ppc"):
            order = ["rd", "imm"] if enc == "li" else ["rd", "ra", "imm"]
            values = [operands[key] for key in order]
        elif arch.startswith("mips"):
            values = ["$" + operands["rt"], "$" + operands["rs"], operands["imm"]]
        else:
            values = ["x" + operands["rd"], "x" + operands["rs1"], operands["imm"]]
        assembly = enc + " " + ",".join(values) + "\n"
        result = subprocess.run([args.llvm_mc, "-triple=" + triples[arch], "-show-encoding"],
                                input=assembly, capture_output=True, text=True, check=True)
        encoding = re.search(r"encoding: \[([^]]+)\]", result.stdout)
        if not encoding:
            raise ValueError(f"{table}:{lineno}: no encoding for {assembly.strip()}")
        actual = bytes(int(byte.strip(), 16) for byte in encoding[1].split(","))
        if actual != bytes.fromhex(expected):
            raise ValueError(f"{table}:{lineno}: {actual.hex(' ')} != {expected}")
        count += 1
    if not count:
        raise ValueError("empty encoding fixture table")
    print(f"{count} fixtures agree with {args.llvm_mc}")


if __name__ == "__main__":
    main()
