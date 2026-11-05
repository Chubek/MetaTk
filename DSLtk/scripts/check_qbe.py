#!/usr/bin/env python3
"""Compare QBE and tree execution in each enabled MetaML CLI."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def outcome(binary, engine, source, directory):
    result = subprocess.run(
        [str(Path(binary).resolve()), "--no-stdlib", "--engine", engine, "-e", source],
        cwd=directory, input=b"", capture_output=True, timeout=60,
        env={**os.environ, "TMPDIR": directory},
    )
    return result.returncode, result.stdout, result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp", required=True)
    parser.add_argument("--d")
    parser.add_argument("--metaml")
    args = parser.parse_args()
    base = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
    with tempfile.TemporaryDirectory(prefix="metaml-qbe-cli ", dir=base) as directory:
        helper = Path(directory) / "helper file.mml"
        helper.write_text('val answer := 42\nIO.println("imported")\n')
        sources = [
            ("40 + 2", 0),
            ("val make x := fn y -> x + y\nmake 40 2", 0),
            ("false && (1 / 0 == 0)", 0),
            ('val _ := IO.println("effect")\n42', 0),
            (" + ".join(["1"] * 96), 0),
            ('import ' + json.dumps(str(helper)) + '\nanswer', 0),
            ("1 / 0", 1), ("true + 1", 1), ("val :=", 1),
        ]
        count = 0
        for binary in filter(None, [args.cpp, args.d, args.metaml]):
            for source, status in sources:
                expected = outcome(binary, "tree", source, directory)
                if expected[0] != status:
                    raise SystemExit(f"invalid reference fixture: {source}\n{expected!r}")
                actual = outcome(binary, "qbe", source, directory)
                if actual != expected:
                    raise SystemExit(f"{binary}: {source}\ntree={expected!r}\nqbe={actual!r}")
                count += 1
        if list(Path(directory).iterdir()) != [helper]:
            raise SystemExit("QBE compilation left temporary artifacts behind")
        print(f"QBE CLI differential: {count} comparisons passed")


if __name__ == "__main__":
    main()
