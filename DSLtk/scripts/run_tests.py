#!/usr/bin/env python3
"""Run supported MetaML source fixtures; nonzero exit or 'not ok' is failure."""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def fixtures():
    return (sorted((ROOT / "metalib").glob("*/test.ml"))
            + sorted((ROOT / "tests/integration").glob("*.ml"))
            + [ROOT / f"tests/integration/host/group_{x}.ml" for x in "abc"]
            + [ROOT / "tests/integration/host/containers.ml"])


def run(binary, args, input_data=b""):
    return subprocess.run([str(Path(binary).resolve()), "--no-tty", "--no-history",
                           "--stdlib", str(ROOT / "metalib"), *map(str, args)],
                          cwd=ROOT, input=input_data, capture_output=True, timeout=60)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", required=True, help="C++ REPL executable")
    parser.add_argument("--d", help="optional D REPL executable")
    args = parser.parse_args()
    failures = 0
    for binary in filter(None, [args.cpp, args.d]):
        for path in fixtures():
            result = run(binary, [path])
            good = result.returncode == 0 and b"not ok" not in result.stdout
            print(f"{'PASS' if good else 'FAIL'} {Path(binary).name}: {path.relative_to(ROOT)}")
            if not good:
                failures += 1
                print((result.stdout + result.stderr).decode(errors="replace"))
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
