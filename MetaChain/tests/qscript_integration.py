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
    assert invoke("encode", source).returncode != 0
    assert invoke("unknown", source).returncode != 0
    assert invoke("run", root / "missing").returncode != 0
    for driver in ([repl], [cli, "--repl"]):
        result = subprocess.run(driver, input=b"return 42;\n:quit\n", capture_output=True, timeout=10)
        assert result.returncode == 0 and b"42\n" in result.stdout, result
print("QScript integration passed")
