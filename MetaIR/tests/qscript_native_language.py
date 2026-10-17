# Runs the QScript language tests through the native (Linux x86-64) backend.
#
# Every tests/qscript-language/*.qs is built into an executable with
# `qscript build`. ASMtk emits the instructions; grammar/match programs link
# the static C++ parser runtime. The program must print its .out text, without
# the final "result N" line, and exit with N modulo 256. Every errors/*.qs must fail at compile time,
# or at run time with a nonzero status and its .err text on standard error.
# Traps raised by the instruction set itself (assert, division, a return with
# no target) report only a status, so they are checked by exit code alone.
import os
import pathlib
import re
import subprocess
import sys
import tempfile

cli = sys.argv[1]
runtime_enabled = len(sys.argv) < 3 or sys.argv[2].upper() in {"ON", "1", "TRUE"}
here = pathlib.Path(__file__).resolve().parent / "qscript-language"

# Negative programs that report their text only through the VM, or that trap in the instruction set.
STATUS_ONLY = {"runtime_assert"}

failures = []

def run(command, **kwargs):
    return subprocess.run(command, capture_output=True, text=True, timeout=120, **kwargs)

def build(source, exe):
    return run([cli, "build", str(source), "-o", str(exe)])


def explicitly_disabled(source, built, root):
    if runtime_enabled or built.returncode == 0 or "QSCRIPT_BUILD_NATIVE_RUNTIME" not in built.stderr:
        return False
    assembly = run([cli, "native", str(source)])
    object_file = run([cli, "object", str(source), "-o", str(root / "disabled.o")])
    if assembly.returncode or "QScript static parser runtime ABI:" not in assembly.stdout or object_file.returncode:
        failures.append(f"{source.stem}: disabled runtime must still permit assembly/object emission")
    return True

programs = sorted(here.glob("*.qs"))
assert programs, "no language tests found"
errors = sorted((here / "errors").glob("*.qs"))
assert errors, "no negative language tests found"

with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)

    for source in programs:
        name = source.stem
        text = source.with_suffix(".out").read_text()
        match = re.search(r"^result (-?\d+)\n\Z", text, re.M)
        assert match, f"{name}.out lacks its final result line"
        expected = text[:match.start()]
        status = int(match.group(1)) & 0xFF
        exe = root / name
        built = build(source, exe)
        if explicitly_disabled(source, built, root):
            continue
        if built.returncode != 0:
            failures.append(f"{name}: build failed\n{built.stdout}{built.stderr}")
            continue
        os.chmod(exe, 0o755)
        result = run([str(exe)])
        if result.returncode != status or result.stdout != expected or result.stderr:
            failures.append(f"{name}: native exit {result.returncode}, expected {status} and the .out text\n"
                            f"{result.stdout}{result.stderr}")

    for source in errors:
        name = source.stem
        expected = source.with_suffix(".err").read_text().strip()
        built = build(source, root / name)
        if explicitly_disabled(source, built, root):
            continue
        if built.returncode != 0:
            if expected not in built.stdout + built.stderr:
                failures.append(f"{name}: compile diagnostic lacks {expected!r}\n{built.stdout}{built.stderr}")
            continue
        exe = root / name
        os.chmod(exe, 0o755)
        result = run([str(exe)])
        if result.returncode == 0:
            failures.append(f"{name}: native run succeeded, expected a failure")
        elif name not in STATUS_ONLY and expected not in result.stdout + result.stderr:
            failures.append(f"{name}: native run lacks {expected!r}\n{result.stdout}{result.stderr}")

if failures:
    print("\n\n".join(failures))
    sys.exit(1)
print(f"{len(programs)} programs and {len(errors)} negative tests through the native backend passed")
