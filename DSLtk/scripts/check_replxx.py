#!/usr/bin/env python3
"""Integration contracts for MetaML.c, including the actual replxx terminal UI."""
import argparse
import json
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True)
    binary = str(Path(parser.parse_args().binary).resolve())
    temporary = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
    with tempfile.TemporaryDirectory(prefix="metaml-replxx-", dir=temporary) as directory:
        cwd = Path(directory)
        home = cwd / "home"
        home.mkdir()
        env = {k: v for k, v in os.environ.items() if not k.startswith("METAML_")}
        env.update(HOME=str(home), XDG_STATE_HOME=str(home / "state"), TERM="xterm")
        env.pop("NO_COLOR", None)
        modules = cwd / "modules"
        modules.mkdir()
        libraries = cwd / "libraries"
        libraries.mkdir()
        (modules / "helpers.ml").write_text("square x := x * x\n")
        (modules / "lesson.ml").write_text("square 12\n")
        (libraries / "library.mml").write_text("library_value := 27\n")

        def run(*args, data="", extra=None, stdlib=True, init=False):
            opts = ["--no-history"]
            if stdlib:
                opts += ["--stdlib", str(ROOT / "metalib")]
            if not init:
                opts += ["--no-init"]
            return subprocess.run([binary, *opts, *args], input=data, text=True,
                                  capture_output=True, cwd=cwd, env=env | (extra or {}), timeout=30)

        def good(*args, **kw):
            r = run(*args, **kw)
            assert r.returncode == 0, r
            return r

        assert "Usage: metaml" in good("--help").stdout
        assert "MetaML 1." in good("-V").stdout
        for option in ("-e", "--eval", "-l", "--load", "--type", "-I", "--stdlib",
                       "--history-file", "--color", "--diagnostic-format"):
            r = run(option)
            assert r.returncode == 2 and "needs an argument" in r.stderr, r
        for args in (("--unknown",), ("--color", "bad"), ("--diagnostic-format", "bad"),
                     ("--ast", "--check"), ("--check", "-i"), ("one.ml", "two.ml")):
            assert run(*args).returncode == 2, args
        r = run("--diagnostic-format=json", "--unknown")
        assert json.loads(r.stderr)["code"] == "CLI", r
        assert good("-e", "1 + 2", "--eval=4 + 5").stdout == "3\n9\n"
        assert good("--type", "fn x -> x").stdout == "'a -> 'a\n"
        assert good("--type", "1 -- trailing comment").stdout == "Int\n"
        assert good("--no-stdlib", "-e", "Process.args ()", "--", "input.txt", "--verbose").stdout == '["input.txt", "--verbose"]\n'

        search = {"METAML_PATH": str(cwd / "missing") + ":" + str(modules),
                  "METAML_LIBPATH": str(libraries) + ":" + str(ROOT / "metalib")}
        assert good("-l", "helpers.ml", "lesson.ml", extra=search).stdout == "144\n"
        assert good("-I" + str(modules), "--load", "helpers.ml", "-e", "square 3").stdout == "9\n"
        assert good("-e", 'import "library"\nlibrary_value', extra=search).stdout == "27\n"
        assert good("-e", "square 4", "-l", "helpers.ml", extra=search).stdout == "16\n"
        assert good("-e", "1 + 2", extra=search, stdlib=False).stdout == "3\n"

        sink = cwd / "results.txt"
        sink.write_text("stale\n")
        r = good("-e", 'IO.println "hello"', "-e", "12", extra={"METAML_SINK": str(sink)})
        assert not r.stdout and sink.read_text() == "hello\n()\n12\n", r
        assert run("-e", "12", extra={"METAML_SINK": str(cwd / "absent" / "out")}).returncode == 1
        r = good("--time", "-e", "1 + 2")
        assert r.stdout == "3\n" and "Elapsed:" in r.stderr, r
        r = good("--trace", "-l", str(modules / "helpers.ml"), "-e", "square 5")
        assert r.stdout == "25\n" and "square 5" in r.stderr, r

        # Checking must not execute even imported code or a custom prelude.
        (modules / "effect.mml").write_text('IO.println "MUST_NOT_EXECUTE"\nchecked_value := 42\n')
        check_file = cwd / "check.ml"
        check_file.write_text('import "effect"\nIO.println "MUST_NOT_EXECUTE"\nsquare checked_value\n')
        r = good("--check", "-l", "helpers.ml", str(check_file), extra=search)
        assert not r.stdout and "MUST_NOT_EXECUTE" not in r.stderr, r
        assert good("--check", "-e", "1 / 0").stdout == ""
        (modules / "warning.mml").write_text("unused x := 1\n")
        r = good("--check", "--diagnostic-format=json", "-e", 'import "warning"', extra=search)
        assert len(r.stderr.splitlines()) == 1 and json.loads(r.stderr)["code"] == "M0307", r
        custom = cwd / "custom"
        (custom / "prelude").mkdir(parents=True)
        (custom / "prelude" / "mod.mml").write_text('IO.println "PRELUDE_EFFECT"\ncustom_value := 19\n')
        r = good("--check", "--stdlib", str(custom), "-e", "custom_value")
        assert not r.stdout and not r.stderr, r
        r = good("--ast", "--stdlib", str(custom), "-e", "1 + 2")
        assert "Program" in r.stdout and "PRELUDE_EFFECT" not in r.stdout, r
        r = good("--check", data="1 + 2")
        assert not r.stdout, r
        r = run("--diagnostic-format", "json", "--check", "-e", "1 + true")
        d = json.loads(r.stderr)
        assert r.returncode == 1 and not r.stdout and d["code"] == "M0301" and d["line"] == 1, r
        r = run("--color", "always", "-e", "unknown_name")
        assert "\x1b[31m" in r.stderr and not r.stdout, r
        assert "\x1b" not in run("--color", "never", "-e", "unknown_name").stderr
        assert run("-e", "val broken :=").returncode == 1

        # File checking succeeds/fails without overwriting bindings or stack.
        check_file.write_text('square x := "new"\nnew_binding := 99\nIO.println "MUST_NOT_EXECUTE"\n')
        invalid_file = cwd / "bad.ml"
        invalid_file.write_text("square x := true\n1 + false\n")
        r = good("-q", data=f"""%load helpers.ml
square 5
%check {check_file}
%inspect square
%bindings new_binding
%stack
%reload
%type square 8
%ast square 4
%time square 6
%trace square 7
%clear
%stack
%which helpers.ml
%paths
%modules
%pwd
%history
%help type
%reset
%bindings square
%quit
""", extra=search)
        for text in ("value: <fn>", "Num 'a =>", "[0] 25", "= 36", "= 49", "(stack is empty)",
                     "(no matching bindings)", "(Program", "Session reset.", "%type EXPR"):
            assert text in r.stdout, (text, r)
        assert "MUST_NOT_EXECUTE" not in r.stdout and "square 7" in r.stderr, r
        assert "Elapsed:" in r.stderr and "val square" not in r.stdout, r
        r = run(data=f"%load helpers.ml\n%check {invalid_file}\nsquare 9\n%quit\n", extra=search)
        assert r.returncode == 1 and "= 81" in r.stdout and "M0301" in r.stderr, r
        assert good(data="%paths modules\n%load helpers.ml\nsquare 4\n%quit\n").stdout.endswith("= 16\n")
        r = good(data=f'%cd "{modules}"\n%pwd\n%quit\n')
        assert r.stdout.strip() == str(modules), r
        assert run(data="%help nonexistent\n%stack unexpected\n%type\n%quit\n").returncode == 1
        r = good(data="val multiline := if true then\n  12\nelse 13\nmultiline\n%quit\n")
        assert "= 12" in r.stdout, r
        assert run(data="val unfinished :=\n").returncode == 1
        r = good("-i", "-e", "1 + 2", data="4 + 5\n%quit\n")
        assert r.stdout == "3\n- : Int = 9\n", r

        # Initialization runs only for the REPL; --no-init suppresses it.
        (home / ".metamlrc").write_text(f'%paths "{modules}"\ninit_value := 123\n')
        assert "= 123" in good(data="init_value\n%quit\n", init=True).stdout
        assert run(data="init_value\n%quit\n").returncode == 1
        assert good("-e", "1", init=True).stdout == "1\n"

        # Checking a functor application must not annotate its live runtime AST.
        functors = cwd / "functors.ml"
        functors.write_text('''module type SHOWABLE := sig
  type t
  val show : t -> String
end
module Show_int : SHOWABLE := struct
  type t = Int
  val show := Int.to_string
end
module Label (X : SHOWABLE) := struct
  val render x := "value=" ^ X.show x
end
module Int_label := Label(Show_int)
''')
        check_file.write_text('''module Show_string : SHOWABLE := struct
  type t = String
  val show x := x
end
module String_label := Label(Show_string)
String_label.render "test"
''')
        r = good(data=f"%load {functors}\n%inspect Int_label.render\n%check {check_file}\nInt_label.render 42\n%inspect Int_label.render\n%quit\n")
        assert "<not materialized>" in r.stdout and '"value=42"' in r.stdout and "value: <fn>" in r.stdout, r

        # Exercise completion, UTF-8 highlighting, interruption, and persistent
        # history through a real pseudo-terminal, not mocked replxx callbacks.
        if os.name == "posix":
            import pty
            hist = cwd / "history.txt"
            master, slave = pty.openpty()
            proc = subprocess.Popen([binary, "--no-init", "--stdlib", str(ROOT / "metalib"),
                                     "--history-file", str(hist), "--color", "always"],
                                    stdin=slave, stdout=slave, stderr=slave, cwd=cwd, env=env)
            os.close(slave)
            received = bytearray()
            cursor = 0

            def until(marker, timeout=15):
                nonlocal cursor
                start = cursor
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    found = received.find(marker, start)
                    if found >= 0:
                        cursor = found + len(marker)
                        return bytes(received[start:cursor])
                    ready, _, _ = select.select([master], [], [], max(0, deadline - time.monotonic()))
                    if ready:
                        try:
                            chunk = os.read(master, 65536)
                        except OSError:
                            break
                        if not chunk:
                            break
                        received.extend(chunk)
                raise AssertionError((marker, bytes(received)))

            def send(line, marker=b"metaml> "):
                os.write(master, line)
                return until(marker)

            try:
                until(b"metaml> ")
                send(b"square x := x * x\r", b"val square")
                until(b"metaml> ")
                send(b"%ty\t square 7\r", b"Int\r\n")
                until(b"metaml> ")
                send(b"squa\t 8\r", b"= 64")
                until(b"metaml> ")
                send(b'String.lengt\t "abc"\r', b"= 3")
                until(b"metaml> ")
                (cwd / "lesson space.ml").write_text("file_completion := 73\n")
                send(b'%load "lesson sp\t\r', b"73\r\n")
                until(b"metaml> ")
                send('"λ"\r'.encode(), '"λ"\r\n'.encode())
                until(b"metaml> ")
                send(b"val unfinished :=\r", b"     .. ")
                send(b"\x03")
                send(b"77\r", b"= 77")
                until(b"metaml> ")
                send(b"%qui\t\r", b"%quit")
                proc.wait(timeout=10)
                assert proc.returncode == 0, bytes(received)
                assert b"\x1b[" in received and hist.exists(), bytes(received)
                assert "square" in hist.read_text() and "%type square 7" in hist.read_text()
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.wait()
                os.close(master)

            saved_history = hist.read_text()
            for disabled in (False, True):
                master, slave = pty.openpty()
                args = [binary, "--no-init", "--no-stdlib", "--history-file", str(hist), "--color", "never", "-q"]
                if disabled:
                    args += ["--no-history"]
                proc = subprocess.Popen(args, stdin=slave, stdout=slave, stderr=slave, cwd=cwd, env=env)
                os.close(slave)
                received = bytearray()
                cursor = 0
                try:
                    until(b"metaml> ")
                    if not disabled:
                        # Last entry is %quit; the one before it is 77.
                        send(b"\x1b[A\x1b[A\r", b"= 77")
                        until(b"metaml> ")
                    else:
                        send(b"88\r", b"= 88")
                        until(b"metaml> ")
                    os.write(master, b"%quit\r")
                    proc.wait(timeout=10)
                    assert proc.returncode == 0, bytes(received)
                    if disabled:
                        assert hist.read_text() == saved_history
                    else:
                        saved_history = hist.read_text()
                finally:
                    if proc.poll() is None:
                        proc.kill()
                        proc.wait()
                    os.close(master)

    print("replxx CLI, isolated checking, and terminal contracts passed")


if __name__ == "__main__":
    main()
