"""Generated target freshness, source ownership, and fail-closed mechanisms."""
from copy import deepcopy
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Arch/dsymspec/tools"))
import yaml
from generate_mlvm import DEFAULT_OUTPUT, DEFAULT_SPEC, generate
from dsymspec_core import load_spec

expected = generate(DEFAULT_SPEC)
assert DEFAULT_OUTPUT.read_text() == expected, "stale MLVM RV64 encoder"
spec = load_spec(DEFAULT_SPEC)
scratch = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
with tempfile.TemporaryDirectory(prefix="mlvm-arch-", dir=scratch) as directory:
    tmp = Path(directory)
    policy = spec["backend"]["file"]
    (tmp / policy).parent.mkdir(parents=True)
    (tmp / policy).write_bytes((DEFAULT_SPEC.parent / policy).read_bytes())
    path = tmp / "riscv64.dsymspec.yaml"

    def write(value):
        path.write_text(yaml.safe_dump(value, sort_keys=False))

    # A data change must flow through generation; a hidden parallel opcode
    # table would leave this expression unchanged.
    changed = deepcopy(spec)
    encoder = next(e for e in changed["encoders"] if e["name"] == "add")
    encoder["body"] = encoder["body"].replace("0x00000033", "0x0000003B")
    write(changed)
    output = generate(path)
    assert output != expected and "return 0x0000003B + rd*128" in output

    # Unknown effects must never be silently discarded.
    encoder["body"] = encoder["body"].rstrip() + "\nREQUIRE rd > 0"
    write(changed)
    try:
        generate(path)
    except ValueError as error:
        assert "unsupported MLVM encoder mechanism" in str(error)
    else:
        raise AssertionError("extra encoder statement was ignored")

    changed = deepcopy(spec)
    changed["targets"][0]["features"].remove("m")
    write(changed)
    try:
        generate(path)
    except ValueError as error:
        assert "RV64IM features" in str(error)
    else:
        raise AssertionError("missing target feature was ignored")

print("MLVM Arch generation and fail-closed checks passed")
