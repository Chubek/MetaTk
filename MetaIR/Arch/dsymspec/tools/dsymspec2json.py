#!/usr/bin/env python3
"""Convert .dsymspec.yaml files to the JSON document described in README.md.

Each spec is validated first (schema and semantic checks); nothing is written
for a spec that fails. The output includes the `backend` extension when the
spec references one.

Usage:
    python3 tools/dsymspec2json.py                       # all specs -> build/json/<arch>.json
    python3 tools/dsymspec2json.py -o out/ x86.dsymspec.yaml
    python3 tools/dsymspec2json.py --stdout lanai.dsymspec.yaml
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import dsymspec_core as core  # noqa: E402


def to_text(doc: dict, compact: bool) -> str:
    if compact:
        return json.dumps(doc, ensure_ascii=False, separators=(",", ":")) + "\n"
    return json.dumps(doc, ensure_ascii=False, indent=2) + "\n"


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("files", nargs="*", type=Path, help="spec files (default: all in repo root)")
    ap.add_argument("-o", "--out-dir", type=Path, default=core.REPO_ROOT / "build" / "json",
                    help="directory for <arch>.json files (default: build/json)")
    ap.add_argument("--stdout", action="store_true", help="write one document to stdout")
    ap.add_argument("--no-backend", action="store_true", help="omit the backend extension")
    ap.add_argument("--compact", action="store_true", help="no indentation")
    args = ap.parse_args(argv)

    files = args.files or core.discover_specs()
    if not files:
        print("no .dsymspec.yaml files found", file=sys.stderr)
        return 2
    if args.stdout and len(files) != 1:
        print("--stdout takes exactly one spec", file=sys.stderr)
        return 2

    status = 0
    for path in files:
        try:
            spec = core.require_valid(path)
            doc = core.build_document(spec, path.parent, with_backend=not args.no_backend)
        except core.SpecError as exc:
            print(exc, file=sys.stderr)
            status = 1
            continue
        text = to_text(doc, args.compact)
        if args.stdout:
            sys.stdout.write(text)
            continue
        args.out_dir.mkdir(parents=True, exist_ok=True)
        target = args.out_dir / (core.arch_of(spec) + ".json")
        with open(target, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("wrote %s (%d records)" % (target, sum(len(doc[k]) for k, _ in core.SECTIONS)))
    return status


if __name__ == "__main__":
    sys.exit(main())
