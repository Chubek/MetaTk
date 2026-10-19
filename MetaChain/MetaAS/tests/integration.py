"""Independent JSON parsing, CLI contracts, and real ISA encoding anchors."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

tool, fixture, metaarch = sys.argv[1:]


def run(source, *args, spec=fixture, code=0):
    result = subprocess.run([tool, "--spec", spec, *args, "-"],
                            input=source.encode(), capture_output=True)
    assert result.returncode == code, (result.returncode, result.stderr.decode())
    return result.stdout


assert run("mov r1, -1\nret", "--format", "hex") == b"11 ff c3\n"
obj = json.loads(run(".extern ext\nbranch ext+7\n", "--format", "json"))
assert obj["format"] == "MetaAS" and obj["version"] == 1
assert obj["relocations"][0]["symbol"] == "ext"
assert obj["relocations"][0]["addend"] == 7
assert obj["relocations"][0]["bit_offset"] == 5
assert obj["sections"][0]["bytes"] == "aaa0000005"
run("branch missing", code=1)
run("ret", "--target", "-1", code=2)
run("ret", "--format", "unknown", code=2)
run("ret", "--max-output", "0", code=1)
run("ret", "--max-source", "2", code=1)
run("ret", spec="/nonexistent/metaas-spec", code=3)
for arch, source, expected in [
    ("ppc32", ".insn addi rd=3, ra=3, imm=-1", "3863ffff"),
    ("ppc32", ".insn addi rd=3, ra=3, imm=1", "38630001"),
]:
    actual = run(source, spec=str(Path(metaarch) / "ArchDB" / (arch + ".dsymspec.yaml")))
    assert actual.hex() == expected
with tempfile.TemporaryDirectory(prefix="metaas-") as work:
    output = Path(work) / "out.bin"
    output.write_bytes(b"keep")
    run("mov r0, 999", "-o", str(output), code=1)
    assert output.read_bytes() == b"keep", "failed assembly truncated output"
    run("ret", "-o", str(output))
    assert output.read_bytes() == b"\xc3"
print("CLI and JSON integration passed")
