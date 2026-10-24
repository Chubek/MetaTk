#!/usr/bin/env python3
"""Exercise CLI argument handling, streams and failure recovery."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True)
    args = parser.parse_args()
    binary = str(Path(args.binary).resolve())
    temporary = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
    with tempfile.TemporaryDirectory(prefix="metaml-cli-", dir=temporary) as directory:
        cwd = Path(directory)

        def run(*arguments, data=b""):
            return subprocess.run(
                [binary, "--stdlib", str(ROOT / "metalib"), "--no-history", *arguments],
                input=data, capture_output=True, cwd=cwd, timeout=30)

        for option in ("-e", "--stdlib", "--dump-ast", "--disable"):
            result = run(option)
            assert result.returncode == 2 and b"needs an argument" in result.stderr, result
        result = run("--unknown")
        assert result.returncode == 2 and b"unknown option" in result.stderr, result

        (cwd / "-input.ml").write_text('IO.print (IO.read_all ())\n')
        for data in (b"", b"unterminated", b"a\r\nb\n", b"a\x00b", b"x" * 20000):
            result = run("--", "-input.ml", data=data)
            assert (result.returncode, result.stdout, result.stderr) == (0, data, b""), result

        (cwd / "bad.ml").write_text("val broken :=\n")
        result = run("--dump-ast", "bad.ml")
        assert result.returncode == 1 and b"error[" in result.stderr, result
        assert b"error[" not in result.stdout, result
        result = run("bad.ml", "-e", '"should-not-run"')
        assert result.returncode == 1 and b"should-not-run" not in result.stdout, result

        result = run("--no-tty", "-e", "1 + 2", "-i", data=b"4 + 5\n")
        assert result.returncode == 0 and not result.stderr, result
        assert b"3" in result.stdout and b"9" in result.stdout, result
    print("CLI contracts passed")


if __name__ == "__main__":
    main()
