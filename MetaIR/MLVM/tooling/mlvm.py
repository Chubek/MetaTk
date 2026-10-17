#!/usr/bin/env python3
"""I/O launcher only: MLVM parsing, verification, execution and lowering are QScript."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def literal(text):
    # Encode source as bytes, not Python/JSON Unicode escapes; never splice IR
    # directly into executable QScript syntax.
    return '"' + "".join(chr(b) if 32 <= b < 127 and b not in (34, 92)
                         else f"\\x{b:02x}" for b in text.encode("utf-8")) + '"'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("command", choices=("check", "run", "opt", "analyze", "emit-qscript", "emit-riscv64", "build", "object"))
    p.add_argument("input", type=Path)
    p.add_argument("-o", "--output", type=Path)
    p.add_argument("--qscript", default=os.environ.get("QSCRIPT", str(ROOT / "build/qscript")))
    p.add_argument("--steps", type=int, default=100000, help="MLVM instruction budget")
    p.add_argument("--host-steps", type=int, default=10000000, help="QScript execution budget")
    optimization = p.add_mutually_exclusive_group()
    optimization.add_argument("--optimize", action="store_true", help="block-local constant propagation")
    optimization.add_argument("--global-optimize", action="store_true", help="global must-constant propagation")
    p.add_argument("--jit", action="store_true", help="run the MLVM implementation with QScript's JIT")
    args = p.parse_args()
    if not 0 < args.steps <= 9223372036854775807 or args.host_steps <= 0:
        p.error("step budgets must be positive signed integers")
    if args.command in ("build", "object", "emit-riscv64") and args.output is None:
        p.error("this command requires -o")
    try:
        with args.input.open("rb") as f:
            raw = f.read(1024 * 1024 + 1)
        if len(raw) > 1024 * 1024:
            raise ValueError("MLVM source exceeds 1 MiB")
        source = raw.decode("utf-8")
        prefix = [args.qscript, "-L", str(ROOT / "MLVM/source"), "-L", str(ROOT / "MLVM/targets"),
                  "-L", str(ROOT / "MLVM/generated"), "--steps", str(args.host_steps)]
        if args.jit:
            prefix.append("--jit")
        driver = "use pipeline, compute, serialize, backend, dataflow, riscv64::riscv64;\n"
        mode = 2 if args.global_optimize else int(args.optimize)
        driver += "let code = pipeline::compile(" + literal(source) + f", {0 if args.command == 'opt' else mode});\n"
        if args.command == "check":
            driver += 'print "ok";'
        elif args.command == "run":
            driver += f"print compute::run(code, {args.steps});"
        elif args.command == "opt":
            operation = "propagate" if args.global_optimize else "fold"
            driver += f"use optimization; print serialize::text(optimization::{operation}(code));"
        elif args.command == "analyze":
            driver += "print dataflow::report(code);"
        elif args.command == "emit-riscv64":
            driver += "print riscv64::riscv64::emit(code);"
        else:
            driver += f"print backend::qscript(code, {args.steps});"
        proc = subprocess.run(prefix + ["run", "-"], input=driver, capture_output=True, text=True, timeout=120)
        if proc.returncode:
            sys.stderr.write(proc.stderr + proc.stdout)
            return 1
        trailer = "result 0\n"
        if not proc.stdout.endswith(trailer):
            raise ValueError("unexpected QScript driver output")
        payload = proc.stdout[:-len(trailer)]
        if args.command in ("build", "object"):
            scratch = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
            with tempfile.TemporaryDirectory(prefix="mlvm-", dir=scratch) as directory:
                lowered = Path(directory) / "lowered.qs"
                lowered.write_text(payload)
                result = subprocess.run([args.qscript, args.command, str(lowered), "-o", str(args.output)], timeout=120)
                if result.returncode == 0 and args.command == "build":
                    args.output.chmod(args.output.stat().st_mode | 0o100)
                return int(result.returncode != 0)
        if args.command == "emit-riscv64":
            args.output.write_bytes(bytes(json.loads(payload)))
        elif args.output:
            args.output.write_text(payload)
        else:
            sys.stdout.write(payload)
        return 0
    except (OSError, UnicodeError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"mlvm: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
