# Runs the QScript language tests through the portable C backend.
#
# Every tests/qscript-language/*.qs must emit C that compiles without warnings
# and prints exactly its .out file with exit 0. Grammar/match programs explicitly
# link the QScript runtime; other programs remain standalone C99. Every
# errors/*.qs must fail at compile time or at run time with its .err text.
# A program that fails at run time reports its message on standard error
# through the generated run-time; traps raised by the instruction set itself
# (assert, division) report only a status, so they are checked by exit code.
import pathlib
import subprocess
import sys
import tempfile

cli, cc, runtime = sys.argv[1:4]
sanitizers = len(sys.argv) > 4 and sys.argv[4].upper() in {"ON", "1", "TRUE"}
here = pathlib.Path(__file__).resolve().parent / "qscript-language"

# Negative programs that report their text only through the VM.
STATUS_ONLY = {"runtime_assert"}

failures = []

def run(command, **kwargs):
    return subprocess.run(command, capture_output=True, text=True, timeout=120, **kwargs)


def compile_c(source, exe):
    flags = []
    if "QScript linked runtime:" in source.read_text():
        flags = ["-I", str(here.parent.parent), runtime,
                 "-Wl,-rpath," + str(pathlib.Path(runtime).resolve().parent)]
        if sanitizers:
            flags += ["-fsanitize=address,undefined"]
    return run([cc, "-std=c99", "-Wall", "-Wextra", "-pedantic", "-O2", "-DQSCRIPT_STANDALONE",
                str(source), *flags, "-o", str(exe)])

programs = sorted(here.glob("*.qs"))
assert programs, "no language tests found"
errors = sorted((here / "errors").glob("*.qs"))
assert errors, "no negative language tests found"

with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)

    for source in programs:
        name = source.stem
        expected = source.with_suffix(".out").read_text()
        emitted = run([cli, "c", str(source), "-o", str(root / f"{name}.c")])
        if emitted.returncode != 0:
            failures.append(f"{name}: emit failed\n{emitted.stdout}{emitted.stderr}")
            continue
        exe = root / name
        built = compile_c(root / f"{name}.c", exe)
        if built.returncode != 0 or built.stderr:
            failures.append(f"{name}: generated C does not compile cleanly\n{built.stderr}")
            continue
        result = run([str(exe)])
        if result.returncode != 0 or result.stdout != expected:
            failures.append(f"{name}: C run exit {result.returncode}, expected exit 0 and the .out text\n"
                            f"{result.stdout}{result.stderr}")

    for source in errors:
        name = source.stem
        expected = source.with_suffix(".err").read_text().strip()
        emitted = run([cli, "c", str(source), "-o", str(root / f"{name}.c")])
        if emitted.returncode != 0:
            if expected not in emitted.stdout + emitted.stderr:
                failures.append(f"{name}: compile diagnostic lacks {expected!r}\n{emitted.stdout}{emitted.stderr}")
            continue
        exe = root / name
        built = compile_c(root / f"{name}.c", exe)
        if built.returncode != 0 or built.stderr:
            failures.append(f"{name}: generated C does not compile cleanly\n{built.stderr}")
            continue
        result = run([str(exe)])
        if result.returncode == 0:
            failures.append(f"{name}: C run succeeded, expected a failure")
        elif name not in STATUS_ONLY and expected not in result.stdout + result.stderr:
            failures.append(f"{name}: C run lacks {expected!r}\n{result.stdout}{result.stderr}")

if failures:
    print("\n\n".join(failures))
    sys.exit(1)
print(f"{len(programs)} programs and {len(errors)} negative tests through the C backend passed")
