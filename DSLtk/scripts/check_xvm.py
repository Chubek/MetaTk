#!/usr/bin/env python3
"""Differentially execute real MetaML fixtures under the retained and XVM engines."""
import argparse
from pathlib import Path
import random
from run_tests import fixtures, run


def outcome(result):
    return result.returncode, result.stdout, result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", required=True)
    parser.add_argument("--d")
    args = parser.parse_args()
    sources = [
        "val loop n acc := if n == 0 then acc else loop (n - 1) (acc + 1)\nloop 100000 0",
        "val f x := fn y -> x - y\nval g := f 50\ng 8",
        "let rec even n := if n == 0 then true else odd (n - 1) and odd n := if n == 0 then false else even (n - 1) in even 10000",
        "false && (1 / 0 == 0)", "true || (1 / 0 == 0)",
        "1 / 0", "true + 1", "match None with | Some n -> n",
        "let r := {x := 1, y := 2} in ({r with x := 42}).x",
    ]
    rng = random.Random(0x58564D)
    sources.append(" + ".join(["1"] * 1200))  # iterative lowering, beyond the former recursion cap
    for _ in range(20):
        a, b, c = [rng.randrange(-10000, 10000) for _ in range(3)]
        def ml(n):
            return f"(~{-n})" if n < 0 else str(n)
        sources.append(f"if {ml(a)} < {ml(b)} then {ml(a)} * {ml(c)} else {ml(b)} - {ml(c)}")
    comparisons = 0
    negative = {"1 / 0", "true + 1", "match None with | Some n -> n"}
    for binary in filter(None, [args.cpp, args.d]):
        probe = run(binary, ["--engine", "jit", "-e", "42"])
        engines = ["xvm", "portable"]
        if probe.returncode == 0:
            engines.append("jit")
        elif b"native executable memory is unavailable" not in probe.stdout + probe.stderr:
            raise SystemExit(f"unexpected native probe failure: {probe.stdout!r} {probe.stderr!r}")
        else:
            print(f"SKIP native execution on {Path(binary).name}: executable memory unavailable")
        invalid = run(binary, ["--engine", "unknown", "-e", "42"])
        if invalid.returncode != 2:
            raise SystemExit("invalid engine must be a usage error")
        for arguments in [[path] for path in fixtures()] + [["-e", source] for source in sources]:
            expected = run(binary, ["--engine", "tree", *arguments])
            # Negative differential cases are intentional; ordinary fixtures must pass.
            if len(arguments) == 1 and expected.returncode:
                raise SystemExit(f"invalid reference fixture: {arguments}")
            if len(arguments) == 2 and (expected.returncode != 0) != (arguments[1] in negative):
                raise SystemExit(f"unexpected reference status: {arguments}\n{outcome(expected)!r}")
            for engine in engines:
                actual = run(binary, ["--engine", engine, *arguments])
                if outcome(actual) != outcome(expected):
                    raise SystemExit(f"{Path(binary).name} / {engine}: {arguments}\n"
                                     f"tree={outcome(expected)!r}\nxvm={outcome(actual)!r}")
                comparisons += 1
    print(f"XVM differential: {comparisons} comparisons passed")


if __name__ == "__main__":
    main()
