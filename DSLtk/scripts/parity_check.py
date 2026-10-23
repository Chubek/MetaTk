#!/usr/bin/env python3
"""Compare stdout, stderr, status and ASTs of the supported D/C++ surface."""
import argparse
import difflib
from pathlib import Path
from run_tests import ROOT, fixtures, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", default="build/metaml-repl-cpp")
    parser.add_argument("--d", default="build/metaml-repl-d")
    parser.add_argument("--trace", action="store_true", help="also compare reduction traces")
    args = parser.parse_args()
    failures = 0
    sources = fixtures() + sorted((ROOT / "metalib").glob("*/mod.ml"))
    for path in sources:
        # mod.ml units are already evaluated by prelude loading. Re-evaluating
        # an ascribed module in its own existing scope is not an isolated test.
        for ast in ((True,) if path.name == "mod.ml" else (False, True)):
            cmd = ["--dump-ast", path] if ast else [path]
            a, b = run(args.cpp, cmd), run(args.d, cmd)
            same = (a.returncode, a.stdout, a.stderr) == (b.returncode, b.stdout, b.stderr)
            good = same and a.returncode == 0
            if not good:
                failures += 1
                print(f"FAIL {'AST' if ast else 'eval'}: {path.relative_to(ROOT)}")
                print(f"exit codes: cpp={a.returncode}, d={b.returncode}")
                print("".join(difflib.unified_diff(
                    (a.stdout + a.stderr).decode(errors="replace").splitlines(True),
                    (b.stdout + b.stderr).decode(errors="replace").splitlines(True),
                    fromfile="cpp", tofile="d")))
    # Malformed input must fail identically (not crash or escape as a host error).
    for source in ["$99999999999999999999999", "(#99999999999999999999999 + 1)",
                   "1_", "1.0__2", 'Float.of_string "_"', "1 / 0"]:
        a, b = run(args.cpp, ["-e", source]), run(args.d, ["-e", source])
        if (a.returncode, a.stdout, a.stderr) != (b.returncode, b.stdout, b.stderr) or a.returncode != 1:
            failures += 1
            print(f"FAIL malformed input: {source}")
    if args.trace:
        a, b = (run(binary, ["-e", "inc x := x + 1", "-e", "#trace inc 41"])
                for binary in [args.cpp, args.d])
        if (a.returncode, a.stdout, a.stderr) != (b.returncode, b.stdout, b.stderr) or a.returncode:
            failures += 1
            print("FAIL trace")
    a, b = run(args.cpp, ["--list-primitives"]), run(args.d, ["--list-primitives"])
    manifest = ROOT / "tests/golden/primitive_manifest.expected"
    if ((a.returncode, a.stdout, a.stderr) != (b.returncode, b.stdout, b.stderr)
            or a.returncode or not manifest.exists() or a.stdout != manifest.read_bytes()):
        failures += 1
        print("FAIL primitive manifest (dialect mismatch or stale golden file)")
    golden = ROOT / "tests/golden/division_recovery"
    for binary in [args.cpp, args.d]:
        result = run(binary, [], golden.with_suffix(".input").read_bytes())
        if result.returncode != 1 or result.stderr or result.stdout != golden.with_suffix(".expected").read_bytes():
            failures += 1
            print(f"FAIL piped diagnostic/recovery golden test: {binary}")
    print(f"Parity: {len(sources)} source files, evaluation + AST, {failures} failure(s)")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
