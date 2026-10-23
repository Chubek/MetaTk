#!/usr/bin/env python3
"""Refresh the authored 45-chapter manual's index, navigation and API blocks."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MANUAL = ROOT / "MetaML-Manual"
API = re.compile(r"<!-- api:begin -->.*?<!-- api:end -->", re.S)
NAV = re.compile(r"\n<!-- nav:begin -->.*?<!-- nav:end -->\n?", re.S)
PORTABLE = {25: "int", 26: "float", 27: "string", 28: "list", 29: "option", 30: "result"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", help="refresh primitive manifest from this built REPL")
    parser.add_argument("--check", action="store_true", help="fail if generated content is stale")
    args = parser.parse_args()
    files = sorted(MANUAL.glob("[0-9][0-9]-*.md"))
    if len(files) != 45:
        raise SystemExit("expected exactly 45 chapters")
    updates = {}
    manifest = ROOT / "tests/golden/primitive_manifest.expected"
    if args.cpp:
        result = subprocess.run([str(Path(args.cpp).resolve()), "--stdlib", str(ROOT / "metalib"),
                                 "--list-primitives"], cwd=ROOT, capture_output=True,
                                text=True, timeout=30, check=True)
        updates[manifest] = result.stdout
    host_api = updates.get(manifest, manifest.read_text() if manifest.exists() else "")
    if not host_api:
        raise SystemExit("generate the manifest first with --cpp <REPL>")
    titles = [f.read_text().splitlines()[0].removeprefix("# ") for f in files]
    index = ("# MetaML and Standard Library Manual\n\n"
             "45 chapters, each in its own Markdown file. Start at chapter 1 or use the index below.\n\n"
             "This manual describes the current tested implementation, including C++, D, the C ABI, "
             "portable libraries, optional host modules, and known limitations.\n\n")
    index += "\n".join(f"- [{title}]({f.name})" for f, title in zip(files, titles)) + "\n"
    updates[MANUAL / "README.md"] = index
    for i, f in enumerate(files):
        text = NAV.sub("\n", f.read_text()).rstrip()
        number = i + 1
        if number in PORTABLE:
            source = ROOT / "metalib" / PORTABLE[number] / "mod.ml"
            sig = source.read_text().split(":= sig\n", 1)[1].split("\nend", 1)[0]
            block = "<!-- api:begin -->\n```text\n" + sig + "\n```\n<!-- api:end -->"
            text = API.sub(lambda _: block, text)
        elif number == 45:
            lines = ["<!-- api:begin -->", "| Primitive | Arity | Registered type |",
                     "| --- | ---: | --- |"]
            for line in host_api.splitlines():
                name, arity, signature = line.split("\t", 2)
                lines.append(f"| `{name}` | {arity} | `{signature.replace('|', '&#124;')}` |")
            lines.append("<!-- api:end -->")
            text = API.sub(lambda _: "\n".join(lines), text)
        nav = ["[Contents](README.md)"]
        if i: nav.append(f"[Previous]({files[i - 1].name})")
        if i + 1 < len(files): nav.append(f"[Next]({files[i + 1].name})")
        updates[f] = text + "\n\n<!-- nav:begin -->\n" + " · ".join(nav) + "\n<!-- nav:end -->\n"
    stale = []
    for path, text in updates.items():
        if not path.exists() or path.read_text() != text:
            stale.append(path.relative_to(ROOT))
            if not args.check:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text)
    if args.check and stale:
        raise SystemExit("stale generated files: " + ", ".join(map(str, stale)))
    print(f"Manual: 45 chapters; {len(host_api.splitlines())} primitive signatures; "
          f"{len(stale)} {'stale' if args.check else 'updated'} files")


if __name__ == "__main__":
    main()
