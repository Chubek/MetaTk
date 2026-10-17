#!/usr/bin/env python3
"""Validate .dsymspec.yaml files against schema/dsymspec.schema.json.

Checks, in order:
  1. YAML parses, with no duplicate mapping keys.
  2. The document matches schema/dsymspec.schema.json (JSON Schema, draft 2020-12).
  3. Semantic checks the schema cannot express:
       - names are unique per record kind;
       - every reference resolves (target features, register parents, form
         mnemonic/encoder/features/syntaxes), as the Lua DSL's emit() did;
       - align values are powers of two;
       - encoder bodies are well-formed pseudo-code;
       - the file name matches the target arch, and the backend file exists,
         parses as JSON, has schema 1 and the same arch.

Usage:
    python3 tools/validate.py                 # every *.dsymspec.yaml in the repo root
    python3 tools/validate.py lanai.dsymspec.yaml x86.dsymspec.yaml

Exit status is 0 when every file is valid, 1 otherwise, 2 on usage errors.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dsymspec_core as core  # noqa: E402


def check_file(path: Path) -> list[str]:
    errors: list[str] = []
    name = path.name
    if not name.endswith(".dsymspec.yaml"):
        errors.append("file name must end in .dsymspec.yaml")
    try:
        spec, found = core.validate_spec(path)
    except core.SpecError as exc:
        return errors + [str(exc)]
    errors.extend(found)
    if not found and name.endswith(".dsymspec.yaml"):
        arch = spec["targets"][0]["arch"]
        if name[: -len(".dsymspec.yaml")] != arch:
            errors.append("file name %r does not match target arch %r" % (name, arch))
    return errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("files", nargs="*", type=Path,
                        help="spec files (default: all *.dsymspec.yaml in the repo root)")
    parser.add_argument("-q", "--quiet", action="store_true", help="print only failures")
    args = parser.parse_args(argv)

    files = args.files or core.discover_specs()
    if not files:
        print("no .dsymspec.yaml files found", file=sys.stderr)
        return 2

    failed = 0
    for path in files:
        if not path.is_file():
            print("%s: no such file" % path, file=sys.stderr)
            return 2
        errors = check_file(path)
        if errors:
            failed += 1
            print("FAIL %s (%d error%s)" % (path.name, len(errors), "" if len(errors) == 1 else "s"))
            for e in errors:
                print("    " + e)
        elif not args.quiet:
            print("ok   %s" % path.name)
    total = len(files)
    print("%d of %d file%s valid" % (total - failed, total, "" if total == 1 else "s"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
