#!/usr/bin/env python3
"""Check 45 chapters, local links, and execute complete MetaML code blocks."""
import argparse
from pathlib import Path
import re
from run_tests import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", required=True)
    parser.add_argument("--d")
    args = parser.parse_args()
    directory = ROOT / "MetaML-Manual"
    files = sorted(directory.glob("[0-9][0-9]-*.md"))
    failures = 0
    examples = 0
    registry = set(re.findall(r"\| (M\d{4}) \|", (ROOT / "manual/errors.md").read_text()))
    for runtime in [ROOT / "MetaML.hpp", ROOT / "MetaML.d"]:
        used = set(re.findall(r'"(M\d{4})"', runtime.read_text()))
        if used - registry:
            failures += 1
            print(f"FAIL unregistered diagnostic codes in {runtime.name}: {sorted(used - registry)}")
    if [int(p.name[:2]) for p in files] != list(range(1, 46)):
        raise SystemExit("expected chapters 01 through 45, each exactly once")
    for path in [directory / "README.md", *files]:
        text = path.read_text()
        prose = re.sub(r"^```.*?^```", "", text, flags=re.M | re.S)
        prose = re.sub(r"`[^`]*`", "", prose)
        for link in re.findall(r"\[[^\]\n]+\]\(([^)]+)\)", prose):
            if "://" not in link and not link.startswith("#"):
                if not (path.parent / link.split("#", 1)[0]).exists():
                    failures += 1
                    print(f"FAIL link {path.name}: {link}")
        for i, source in enumerate(re.findall(r"^```metaml\n(.*?)^```", text, re.M | re.S), 1):
            examples += 1
            for binary in filter(None, [args.cpp, args.d]):
                result = run(binary, ["-e", source])
                if result.returncode or b"not ok" in result.stdout:
                    failures += 1
                    print(f"FAIL {Path(binary).name}: {path.name} example {i}")
                    print((result.stdout + result.stderr).decode(errors="replace"))
    print(f"Manual: {len(files)} chapters, {examples} executable MetaML examples, {failures} failure(s)")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
