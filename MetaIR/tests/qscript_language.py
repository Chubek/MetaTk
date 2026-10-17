# Runs the QScript language tests: every tests/qscript-language/*.qs must exit
# 0 and print exactly its .out file; every errors/*.qs must fail with a
# diagnostic containing the text of its .err file.
import pathlib
import subprocess
import sys

cli = sys.argv[1]
here = pathlib.Path(__file__).resolve().parent / "qscript-language"
failures = []

def run(source):
    return subprocess.run([cli, "run", str(source)], capture_output=True, text=True, timeout=60)

programs = sorted(here.glob("*.qs"))
assert programs, "no language tests found"
for source in programs:
    expected = source.with_suffix(".out")
    assert expected.is_file(), f"missing expected output for {source.name}"
    result = run(source)
    if result.returncode != 0 or result.stdout != expected.read_text():
        failures.append(f"{source.name}: exit {result.returncode}\n{result.stdout}{result.stderr}")

errors = sorted((here / "errors").glob("*.qs"))
assert errors, "no negative language tests found"
for source in errors:
    expected = source.with_suffix(".err").read_text().strip()
    result = run(source)
    text = result.stdout + result.stderr
    if result.returncode == 0 or expected not in text:
        failures.append(f"{source.name}: expected {expected!r}, got exit {result.returncode}: {text.strip()}")

if failures:
    print("\n\n".join(failures))
    sys.exit(1)
print(f"{len(programs)} programs and {len(errors)} negative tests passed")
