#!/usr/bin/env python3
"""Validate the book structure and run independent, explicitly tagged listings."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
LISTING = re.compile(r'book\.code\("([^"]+)",\s*\[=\[(.*?)\]=\],\s*"([^"]+)"\)', re.S)
ROFFLETS = (
    "formal-grammar", "graph", "math-notation", "memo", "pseudo-code",
    "symbol", "syntax-highlight", "uml",
)


def run(command, **kwargs):
    return subprocess.run(command, cwd=REPO, capture_output=True, text=True,
                          timeout=60, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--structure-only", action="store_true")
    parser.add_argument("--metaml", default=str(REPO / "build/metaml-repl-cpp"))
    parser.add_argument("--pplua")
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()
    expected = {f"ch-{n}.me" for n in range(1, 43)}
    actual = {p.name for p in (ROOT / "chapters").glob("*.me")}
    failures, listings, chapters = [], [], []
    if actual != expected:
        failures.append(f"chapter set mismatch: missing {expected - actual}; extra {actual - expected}")
    for n in range(1, 43):
        path = ROOT / f"chapters/ch-{n}.me"
        if not path.exists():
            continue
        text = path.read_text()
        prose = re.sub(r"^\.lua\n.*?^\.endlua\n?", "", text, flags=re.M | re.S)
        prose = re.sub(r"^\..*$", "", prose, flags=re.M)
        words = len(prose.split())
        if words < 650:
            failures.append(f"{path.name}: only {words} prose words; expected a substantial chapter")
        if text.count("\n.endlua") != len(re.findall(r"^\.lua$", text, re.M)):
            failures.append(f"{path.name}: unbalanced Lua blocks")
        if len(re.findall(rf'book\.chapter\({n}, "', text)) != 1:
            failures.append(f"{path.name}: missing unique chapter declaration")
        if len(re.findall(r"^\.sh 2 ", text, re.M)) < 4:
            failures.append(f"{path.name}: expected at least four developed sections")
        if not re.search(r'^\.sh 2 "Sources and further work"', text, re.M):
            failures.append(f"{path.name}: missing source and exercise guidance")
        for language, source, caption in LISTING.findall(text):
            listings.append((path.name, language, source, caption))
        chapters.append({"number": n, "file": str(path.relative_to(ROOT)),
                         "prose_words": words, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    for name in ROFFLETS:
        path = ROOT / "luaROFF/rofflets" / f"{name}.rofflet"
        if not path.exists() or path.stat().st_size < 100:
            failures.append(f"missing implementation: {path}")
    executable = [x for x in listings if x[1] == "metaml" or x[1].startswith("metaml-error:")]
    if len(executable) < 60:
        failures.append(f"only {len(executable)} executable examples; expected at least 60")
    if not args.structure_only:
        binary = Path(args.metaml).resolve()
        if not binary.is_file():
            failures.append(f"MetaML executable not found: {binary}")
        else:
            for filename, language, source, caption in executable:
                try:
                    result = run([str(binary), "--no-tty", "--no-history", "--stdlib",
                                  str(REPO / "metalib"), "-e", source])
                    output = result.stdout + result.stderr
                    if language == "metaml":
                        good = result.returncode == 0 and "not ok" not in output
                        good = good and ('Test.equal' in source or 'Test.check' in source)
                    else:
                        good = result.returncode != 0 and language.split(":", 1)[1] in output
                    if not good:
                        failures.append(f"{filename}: {caption}\n{output}")
                except (OSError, subprocess.TimeoutExpired) as error:
                    failures.append(f"{filename}: {caption}: {error}")
        if args.pplua:
            command = [str(Path(args.pplua).resolve()), "-n", "-D", f"BOOK_ROOT={ROOT}",
                       "-D", "BOOK_FORMAT=pdf", "-e", 'dofile(BOOK_ROOT .. "/macros/book.lua")']
            fixture = (ROOT / "scripts/rofflets-smoke.lroff").read_text()
            try:
                result = run(command, input=fixture)
                if result.returncode or result.stderr or ".PS" not in result.stdout or ".EQ" not in result.stdout:
                    failures.append(f"rofflet integration failure:\n{result.stderr}")
                if r"\&.control" not in result.stdout or r"\e" not in result.stdout:
                    failures.append("listing escape regression: control prefix or backslash lost")
                bad = run(command, input='.lua\nbook.graph({}, {{"Missing", "Other"}}, "bad")\n.endlua\n')
                if "unknown graph endpoint" not in bad.stderr:
                    failures.append("graph validation did not reject unknown endpoints")
            except (OSError, subprocess.TimeoutExpired) as error:
                failures.append(f"rofflet check: {error}")
    if failures:
        for failure in failures:
            print(f"FAIL {failure}", file=sys.stderr)
        return 1
    manifest = {"title": "MetaML: A Meta-Language for Meta-Programming",
                "chapters": chapters, "prose_words": sum(c["prose_words"] for c in chapters),
                "listings": len(listings), "executable_examples": len(executable)}
    if args.manifest:
        args.manifest.write_text(json.dumps(manifest, indent=2) + "\n")
    verb = "inventoried" if args.structure_only else "checked"
    print(f"Book: {len(chapters)} chapters, {manifest['prose_words']} prose words, "
          f"{len(executable)} executable examples {verb}; no failures")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
