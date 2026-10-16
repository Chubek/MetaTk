import pathlib
import platform
import subprocess
import sys
import tempfile

cli, repl = sys.argv[1:]
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    source = root / "factorial.qs"
    source.write_text("let n=5; let f=1; while(n>1) { f=f*n; n=n-1; } print f; return f;")
    def invoke(*args, **kwargs):
        return subprocess.run([cli, *map(str, args)], capture_output=True, timeout=30, **kwargs)
    run = invoke("run", source)
    assert run.returncode == 0, run.stderr
    assert run.stdout == b"120\nresult 120\n", run.stdout
    assert invoke("check", source).stdout == b"ok\n"
    for command in ("ir", "encode", "native", "object", "build"):
        target = root / command
        built = invoke(command, source, "-o", target)
        assert built.returncode == 0, built.stderr
        assert target.stat().st_size
    if platform.system() == "Linux" and platform.machine() == "x86_64":
        (root / "build").chmod(0o700)
        native = subprocess.run([root / "build"], capture_output=True, timeout=10)
        assert native.returncode == 120 and native.stdout == b"120\n", native
    assert invoke("run", "-", input=b"return 7;").stdout == b"result 7\n"
    assert invoke("run", "-", input=b"return missing;").returncode != 0
    # Multi-file modules: earlier files are a prelude for later ones.
    prelude = root / "prelude.qs"
    prelude.write_text("let base = 40;")
    user = root / "user.qs"
    user.write_text("return base + 2;")
    multi = invoke("run", prelude, user)
    assert multi.returncode == 0, multi.stderr
    assert multi.stdout == b"result 42\n", multi.stdout
    assert invoke("check", prelude, user).stdout == b"ok\n"
    assert invoke("run", prelude, "-", input=b"return 1;").returncode != 0
    # Bundled examples execute; the prelude is a library of constants.
    here = pathlib.Path(__file__).resolve().parent.parent
    examples = here / "qscript" / "examples"
    for example in sorted(examples.glob("*.qs")):
        example_run = invoke("run", example)
        assert example_run.returncode == 0, (example, example_run.stderr)
    prelude_user = root / "prelude_user.qs"
    prelude_user.write_text("use prelude; let n=5; let f=TRUE; while(n>1) { f=f*n; n=n-1; } print f; return f;")
    prelude_run = invoke("run", prelude_user)
    assert prelude_run.returncode == 0, prelude_run.stderr
    assert prelude_run.stdout == run.stdout, (prelude_run.stdout, run.stdout)
    # Every standard library has a self-checking test program.
    libs = invoke("libs").stdout.decode().split()
    assert len(libs) >= 24, libs
    tests = here / "tests" / "qscript-stdlib"
    for lib in libs:
        test = tests / (lib + ".qs")
        assert test.is_file(), ("missing stdlib test", lib)
        result = invoke("run", test)
        assert result.returncode == 0 and result.stdout == b"1\nresult 0\n", (lib, result.stdout, result.stderr)
        assert invoke("lib", lib).stdout, lib
    # Libraries from a directory with -L; functions callable from the CLI.
    libdir = root / "lib"
    (libdir / "geo").mkdir(parents=True)
    (libdir / "geo" / "area.qs").write_text("fn rect(w, h) { return w * h; }")
    user_lib = root / "uselib.qs"
    user_lib.write_text("use geo::area; fn twice(x) { return 2 * x; } return geo::area::rect(6, 7);")
    assert invoke("-L", libdir, "run", user_lib).stdout == b"result 42\n"
    assert invoke("run", user_lib).returncode != 0
    assert invoke("-L", libdir, "call", user_lib, "--", "twice", "21").stdout == b"42\n"
    assert invoke("--no-stdlib", "run", "-", input=b"use math; return 0;").returncode != 0
    # Introspection commands.
    assert b"grammar QScript" in invoke("grammar").stdout
    assert invoke("ast", source).stdout.startswith(b"(program (let")
    assert invoke("tree", source).stdout.startswith(b"(TOP (item")
    assert b"total makespan" in invoke("report", source).stdout
    grammar = root / "pairs.grammar"
    grammar.write_text("grammar Pairs { rule TOP { <pair>+ % ',' } token pair { <ident> '=' \\d+ } }")
    text = root / "pairs.txt"
    text.write_text("a=1, b=2")
    parsed = invoke("parse", grammar, text)
    assert parsed.returncode == 0 and parsed.stdout == b'(TOP (pair (ident "a")) (pair (ident "b")))\n', parsed
    # Engines, schedulers and limits.
    for flags in (["--jit"], ["--sched", "cycle"], ["--sched", "static"], ["-O0"]):
        assert invoke(*flags, "run", source).stdout == run.stdout, flags
    assert invoke("--steps", "10", "run", source).returncode != 0
    stats = invoke("--stats", "run", source)
    assert b"steps" in stats.stderr
    # Diagnostics carry file:line:column.
    broken = root / "broken.qs"
    broken.write_text("let x = 1;\nreturn x\n")
    failed = invoke("run", broken)
    assert failed.returncode != 0 and str(broken).encode() + b":3:1: expected ';'" in failed.stderr, failed.stderr
    # Functions, recursion and libraries compile to native code too.
    fib = root / "fib.qs"
    fib.write_text("use math; fn fib(n) { if (n < 2) return n; return fib(n - 1) + fib(n - 2); } print fib(15); return gcd(fib(12), 100);")
    assert invoke("run", fib).stdout == b"610\nresult 4\n"
    assert invoke("build", fib, "-o", root / "fib").returncode == 0
    if platform.system() == "Linux" and platform.machine() == "x86_64":
        (root / "fib").chmod(0o700)
        native = subprocess.run([root / "fib"], capture_output=True, timeout=10)
        assert native.returncode == 4 and native.stdout == b"610\n", native
    assert invoke("encode", source).returncode != 0
    assert invoke("unknown", source).returncode != 0
    assert invoke("run", root / "missing").returncode != 0
    for driver in ([repl], [cli, "--repl"]):
        result = subprocess.run(driver, input=b"return 42;\n:quit\n", capture_output=True, timeout=10)
        assert result.returncode == 0 and b"42\n" in result.stdout, result
        session = b"use math;\nfn sq(x) { return x * x; }\nreturn sq(gcd(12, 18));\n:quit\n"
        result = subprocess.run(driver, input=session, capture_output=True, timeout=10)
        assert result.returncode == 0 and b"36\n" in result.stdout, result
print("QScript integration passed")
