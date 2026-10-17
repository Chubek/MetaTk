# Runs the metachain driver on the repository's examples and checks the behaviour
# documented in manual/10-text-and-cli.md: command output, round trips through
# binary files, and the exit statuses 0, 1 and 2.
import os
import pathlib
import subprocess
import sys
import tempfile

cli = sys.argv[1]
root = pathlib.Path(__file__).resolve().parent.parent
examples = root / "examples"
failures = []

def run(*args, stdin=None):
    return subprocess.run([cli, *args], capture_output=True, text=True, input=stdin, timeout=120)

def expect(condition, message):
    if not condition:
        failures.append(message)

factorial = str(examples / "factorial.mcil")
restored_check = None

result = run("run", factorial)
expect(result.returncode == 0 and result.stdout == "6\n30\n120\n360\n720\n720\nresult 720\n",
       f"run factorial: exit {result.returncode}, output {result.stdout!r}")
expect(run("check", factorial).stdout == "ok\n", "check factorial does not print ok")
expect('@INT "1"' in run("parse", str(examples / "list.mcg")).stdout, "parse list.mcg lacks the INT token")
expect(run("rewrite", str(examples / "simplify.mcr")).stdout == "42\n", "rewrite simplify.mcr is not 42")
expect(run("run", "--max-steps", "3", factorial).returncode == 1, "step limit 3 did not fail with status 1")

expect(run("build", factorial).returncode == 2, "build without -o did not exit 2")
expect(run("run", factorial, "-o").returncode == 2, "missing -o value did not exit 2")
expect(run("run", "--max-steps", "1x", factorial).returncode == 2, "invalid step limit did not exit 2")
expect(run("frobnicate").returncode == 2, "unknown command did not exit 2")
expect(run("run", "/nonexistent/input.mcil").returncode == 1, "missing input did not exit 1")
expect(run("--help").returncode == 0, "--help did not exit 0")

with tempfile.TemporaryDirectory() as directory:
    work = pathlib.Path(directory)
    executable = work / "factorial"
    built = run("build", factorial, "-o", str(executable))
    expect(built.returncode == 0, f"build factorial failed: {built.stderr}")
    if built.returncode == 0:
        os.chmod(executable, 0o755)
        native = subprocess.run([str(executable)], capture_output=True, text=True, timeout=60)
        expect(native.returncode == 720 % 256 and native.stdout == "6\n30\n120\n360\n720\n720\n",
               f"native factorial: exit {native.returncode}, output {native.stdout!r}")

    encoded = work / "factorial.mcilb"
    restored = work / "restored.mcil"
    expect(run("encode", factorial, "-o", str(encoded)).returncode == 0, "encode factorial failed")
    expect(run("decode", str(encoded), "-o", str(restored)).returncode == 0, "decode failed")
    expect(run("run", str(restored)).stdout.endswith("result 720\n"), "decoded program does not run")

    assembly = work / "factorial.s"
    objectfile = work / "factorial.o"
    linked = work / "linked"
    expect(run("native", factorial, "--no-optimize", "-o", str(assembly)).returncode == 0, "native failed")
    expect(run("object", str(assembly), "-o", str(objectfile)).returncode == 0, "object failed")
    expect(run("link", str(objectfile), "-o", str(linked)).returncode == 0, "link failed")
    if linked.exists():
        os.chmod(linked, 0o755)
        expect(subprocess.run([str(linked)], capture_output=True, timeout=60).returncode == 720 % 256,
               "object-to-link path did not produce the expected status")

if failures:
    print("\n".join(failures))
    sys.exit(1)
print("metachain driver checks passed")
