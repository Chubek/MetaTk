"""Compile the QScript MLVM implementation itself to C99 and native code."""
from pathlib import Path
import platform
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
cli, cc, runtime, native_enabled, sanitizers = sys.argv[1:6]
native = native_enabled.upper() in {"ON", "1", "TRUE"}
native = native and sys.platform.startswith("linux") and platform.machine() in {"x86_64", "amd64"}
sanitizers = sanitizers.upper() in {"ON", "1", "TRUE"}
prefix = [cli, "-L", str(ROOT / "MLVM/source")]


def run(command):
    return subprocess.run(command, capture_output=True, text=True, timeout=120)


def success(result):
    assert result.returncode == 0, result.stdout + result.stderr
    return result


scratch = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
with tempfile.TemporaryDirectory(prefix="mlvm-runtime-", dir=scratch) as directory:
    tmp = Path(directory)
    for name, output in (("core", "MLVM core passed"), ("dataflow", "MLVM dataflow passed"),
                         ("register", "MLVM register allocation passed"), ("metair", "MLVM MetaIR conversion passed")):
        source = ROOT / f"MLVM/tests/{name}.qs"
        cfile, executable = tmp / f"{name}.c", tmp / f"{name}-c"
        success(run(prefix + ["c", str(source), "-o", str(cfile)]))
        flags = ["-I", str(ROOT), runtime, "-Wl,-rpath," + str(Path(runtime).resolve().parent)]
        if sanitizers:
            flags += ["-fsanitize=address,undefined"]
        built = success(run([cc, "-std=c99", "-Wall", "-Wextra", "-pedantic", "-DQSCRIPT_STANDALONE",
                             str(cfile), *flags, "-o", str(executable)]))
        assert not built.stderr, built.stderr
        result = success(run([str(executable)]))
        assert result.stdout == output + "\nresult 0\n", result.stdout + result.stderr
        if native:
            executable = tmp / f"{name}-native"
            success(run(prefix + ["build", str(source), "-o", str(executable)]))
            executable.chmod(executable.stat().st_mode | 0o100)
            result = success(run([str(executable)]))
            assert result.stdout == output + "\n", result.stdout + result.stderr

print("MLVM implementation passed through linked C99" + (" and static native executables" if native else ""))
