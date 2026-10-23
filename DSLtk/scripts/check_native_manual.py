#!/usr/bin/env python3
"""Compile and execute the manual's C++, D and C embedding examples."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile
from run_tests import ROOT


def command(args):
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True, capture_output=True, timeout=120)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--d-compiler")
    args = parser.parse_args()
    base = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
    with tempfile.TemporaryDirectory(prefix="metaml-manual-", dir=base) as temp:
        temp = Path(temp)
        for number, language, extension in [(41, "cpp", "cpp"), (42, "d", "d"), (43, "c", "c")]:
            if language == "d" and not args.d_compiler:
                continue
            chapter = next((ROOT / "MetaML-Manual").glob(f"{number}-*.md"))
            source = re.search(rf"^```{language}\n(.*?)^```", chapter.read_text(), re.M | re.S)[1]
            path = temp / f"example{number}.{extension}"
            path.write_text(source)
            binary = temp / f"example{number}"
            if language == "cpp":
                command([args.cxx, "-std=c++20", "-O0", "-g0", "-pthread", f"-I{ROOT}", path, "-o", binary])
            elif language == "d":
                output = ["-o", binary] if "gdc" in args.d_compiler else [f"-of={binary}"]
                command([args.d_compiler, f"-I{ROOT}", path, ROOT / "MetaML.d", ROOT / "DSLtk.d", *output])
            else:
                obj = temp / "example43.o"
                command([args.cc, "-std=c99", "-Werror", f"-I{ROOT}", "-c", path, "-o", obj])
                command([args.cxx, obj, Path(args.library).resolve(), "-pthread", "-o", binary])
            result = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, timeout=30, check=True)
            if result.stdout.strip() != b"42":
                raise SystemExit(f"unexpected output from {chapter.name}: {result.stdout!r}")
            print(f"PASS native {chapter.name}")


if __name__ == "__main__":
    main()
