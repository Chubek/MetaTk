"""Black-box MLVM tests. The implementation under test is QScript, not Python."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CLI = str(Path(sys.argv[1]).resolve())
CC = sys.argv[2] if len(sys.argv) > 2 else "cc"
PREFIX = [CLI, "-L", str(ROOT / "MLVM/source"), "-L", str(ROOT / "MLVM/targets"),
          "-L", str(ROOT / "MLVM/generated")]
sys.path.insert(0, str(ROOT / "MLVM/tooling"))
from mlvm import literal


def command(argv, **kwargs):
    return subprocess.run(argv, capture_output=True, text=True, timeout=120, **kwargs)


def success(proc):
    assert proc.returncode == 0, proc.stdout + proc.stderr
    return proc.stdout


def qs(source, jit=False):
    return command(PREFIX + (["--jit"] if jit else []) + ["run", "-"], input=source)


def negative(source, diagnostic):
    proc = qs(source)
    assert proc.returncode != 0 and diagnostic in proc.stdout + proc.stderr, (
        diagnostic, proc.returncode, proc.stdout, proc.stderr)


for jit in (False, True):
    out = success(command(PREFIX + (["--jit"] if jit else []) + ["run", str(ROOT / "MLVM/tests/core.qs")]))
    assert out == "MLVM core passed\nresult 0\n", out
    out = success(command(PREFIX + (["--jit"] if jit else []) + ["run", str(ROOT / "MLVM/tests/dataflow.qs")]))
    assert out == "MLVM dataflow passed\nresult 0\n", out
    out = success(command(PREFIX + (["--jit"] if jit else []) + ["run", str(ROOT / "MLVM/tests/register.qs")]))
    assert out == "MLVM register allocation passed\nresult 0\n", out
    out = success(command(PREFIX + (["--jit"] if jit else []) + ["run", str(ROOT / "MLVM/tests/metair.qs")]))
    assert out == "MLVM MetaIR conversion passed\nresult 0\n", out

# Layer boundaries use QScript's actual import/privacy rules, not launcher
# tricks or globally concatenating every implementation into one namespace.
negative("use pipeline; print ir::zeros(1);", "not imported")
negative("use riscv64::riscv64; riscv64::riscv64::_word([], 0);", "undefined function")
assert success(qs('use pipeline, backend, amd64::amd64; let code = pipeline::compile("module 1 { const r0 1; ret r0; }", 0); assert backend::qscript(code, 10) == amd64::amd64::lower(code, 10);')) == "result 0\n"

bad_modules = [
    ("module 1 {}", "invalid module size"),
    ("module 2 { ret r0; }", "MLVM syntax:"),
    ("module 1 { nope r0; }", "unknown operation"),
    ("module 1 { const r32 1; ret r0; }", "register out of range"),
    ("module 1 { const r0 1; }", "fallthrough out of range"),
    ("module 1 { ret r0; }", "undefined register"),
    ("module 1 { jmp -1; }", "branch or fallthrough out of range"),
    ("module 1 { const 1 r0; ret r0; }", "expected register operand"),
    ("module 1 { const r0 1 2; ret r0; }", "wrong operands"),
    ("module 1 { const r0 9223372036854775808; ret r0; }", "integer"),
    ("module 1 { const r0 0; jz r0 4; const r1 1; jmp 4; ret r1; }", "undefined register"),
    ("module 1 { add r1 r1 r0; const r0 1; const r1 1; jmp 0; }", "undefined register"),
    ("module 1 { const r0 1; ret r0; jmp 99; }", "branch or fallthrough out of range"),
    ("module 1 { const r0 1; const r1 0; div r0 r0 r1; ret r0; }", "invalid division"),
    ("module 1 { const r0 -9223372036854775808; const r1 -1; mod r0 r0 r1; ret r0; }", "invalid division"),
    ("module 1 { const r0 1; const r1 64; shl r0 r0 r1; ret r0; }", "invalid division"),
    ("module 1 { jmp 0; }", "MLVM: step limit exceeded"),
]
for text, diagnostic in bad_modules:
    negative("use pipeline; print pipeline::run(" + literal(text) + ", 100, 1);", diagnostic)
    negative("use pipeline; print pipeline::run(" + literal(text) + ", 100, 2);", diagnostic)

for library, operation in (("control_flow", "reachable"), ("control_flow", "predecessors"),
                           ("control_flow", "blocks"), ("control_flow", "reverse_postorder"),
                           ("control_flow", "immediate_dominators"), ("dataflow", "liveness"),
                           ("dataflow", "report"), ("optimization", "constants"), ("optimization", "propagate")):
    negative(f"use {library}; {library}::{operation}([[99, 0, 0, 0, 0]]);", "invalid opcode")
negative('use pipeline; pipeline::run("module 1 { const r0 5; const r1 0; const r2 1; add r1 r1 r0; sub r0 r0 r2; jz r0 7; jmp 3; ret r1; }", 22, 2);', "step limit exceeded")
negative("use register; register::interference([[99, 0, 0, 0, 0]]);", "invalid opcode")
negative('use metair; metair::import_mcil("MCIL");', "invalid MCIL header")
negative('use metair; metair::import_mcil("MCIL\\x01\\0\\0\\0\\0\\0\\0\\0");', "invalid MCIL length")
negative("use metair; metair::export_mcil([[99, 0, 0, 0, 0]]);", "invalid opcode")
negative("use register; register::color([[99, 0, 0, 0, 0]], 2);", "invalid opcode")
negative("use register; register::color([[0, 0, 0, 0, 1], [16, 0, 0, 0, 0]], 0);", "capacity must be 1..32")
negative("use register; register::color([[0, 0, 0, 0, 1], [16, 0, 0, 0, 0]], 33);", "capacity must be 1..32")
negative('use pipeline, register; register::color(pipeline::compile("module 1 { const r0 42; const r31 99; ret r0; }", 0), 1);', "allocation exhausted")

for source, diagnostic in [
    ("use control_flow; control_flow::verify([[0, 1]]);", "five fields"),
    ("use control_flow; control_flow::verify([[99, 0, 0, 0, 0]]);", "invalid opcode"),
    ("use control_flow; control_flow::verify([[16, 1, 0, 0, 0]]);", "noncanonical"),
    ("use bytecode; bytecode::decode([0, 1, 1]);", "invalid bytecode header"),
    ("use bytecode; bytecode::decode([1296848461, 2, 1]);", "invalid bytecode header"),
    ("use bytecode; bytecode::decode([1296848461, 1, 9223372036854775807]);", "invalid bytecode length"),
    ("use bytecode; bytecode::decode([1296848461, 1, 1, 16, 0, 0, 0, 0, 0]);", "invalid bytecode length"),
    ("use pipeline; pipeline::run(\"module 1 { const r0 1; ret r0; }\", 0, 0);", "step limit exceeded"),
]:
    negative(source, diagnostic)

answer = (ROOT / "MLVM/tests/answer.mlvm").read_text()
raw_driver = "use pipeline, riscv64::riscv64; print riscv64::riscv64::emit(pipeline::compile(" + literal(answer) + ", 0));"
raw_output = success(qs(raw_driver))
raw = bytes(json.loads(raw_output.removesuffix("result 0\n")))
# Independent ISA reference for the five emitted words (RV64I).
assert raw.hex() == "13054001930560013305b5001305050067800000", raw.hex()

# High virtual names no longer imply high physical register pressure.
high = success(qs('use pipeline, riscv64::riscv64; print riscv64::riscv64::emit(pipeline::compile("module 1 { const r31 42; ret r31; }", 0));'))
assert bytes(json.loads(high.removesuffix("result 0\n"))).hex() == "1305a0021305050067800000"
pressure = "module 1 { " + " ".join(f"const r{r} 1;" for r in range(9))
pressure += " " + " ".join(f"add r0 r0 r{r};" for r in range(1, 9)) + " ret r0; }"

for text, diagnostic in [
    ("module 1 { const r0 2048; ret r0; }", "immediate out of range"),
    (pressure, "allocation exhausted"),
    ("module 1 { const r0 1; jmp 2; ret r0; }", "unsupported operation"),
    ("module 1 { const r0 1; div r1 r0 r0; ret r1; }", "unsupported operation"),
]:
    negative("use pipeline, riscv64::riscv64; print riscv64::riscv64::emit(pipeline::compile(" + literal(text) + ", 0));", diagnostic)

scratch = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
with tempfile.TemporaryDirectory(prefix="mlvm-tests-", dir=scratch) as directory:
    tmp = Path(directory)
    # Compare to an external assembler when LLVM's cross tools are installed.
    if shutil.which("clang") and shutil.which("llvm-objcopy"):
        asm = tmp / "reference.s"
        asm.write_text(".text\n.option norvc\naddi a0,zero,20\naddi a1,zero,22\nadd a0,a0,a1\naddi a0,a0,0\nret\n")
        obj, binary = tmp / "reference.o", tmp / "reference.bin"
        cross = command(["clang", "--target=riscv64", "-march=rv64im", "-c", str(asm), "-o", str(obj)])
        if cross.returncode == 0:
            success(command(["llvm-objcopy", "-O", "binary", "--only-section=.text", str(obj), str(binary)]))
            assert binary.read_bytes() == raw
        else:
            print("SKIP external RV64 reference: clang lacks RISC-V support")
    for fixture, expected in (("answer", 42), ("sum", 15), ("diamond", 42)):
        text = (ROOT / f"MLVM/tests/{fixture}.mlvm").read_text()
        source = success(qs("use pipeline, backend; print backend::qscript(pipeline::compile(" + literal(text) + ", 2), 100);"))
        lowered = tmp / f"{fixture}.qs"
        lowered.write_text(source.removesuffix("result 0\n"))
        for jit in (False, True):
            assert success(command([CLI] + (["--jit"] if jit else []) + ["run", str(lowered)])) == f"result {expected}\n"
        cfile, executable = tmp / f"{fixture}.c", tmp / f"{fixture}-c"
        success(command([CLI, "c", str(lowered), "-o", str(cfile)]))
        success(command([CC, "-std=c99", "-Wall", "-Wextra", "-pedantic", "-DQSCRIPT_STANDALONE", str(cfile), "-o", str(executable)]))
        result = command([str(executable)])
        assert result.returncode == 0 and result.stdout == f"result {expected}\n", result
        if sys.platform.startswith("linux") and __import__("platform").machine() in ("x86_64", "amd64"):
            native = tmp / f"{fixture}-native"
            success(command([CLI, "build", str(lowered), "-o", str(native)]))
            native.chmod(native.stat().st_mode | 0o100)
            result = command([str(native)])
            assert result.returncode == expected and result.stdout == "", result

    launcher = [sys.executable, str(ROOT / "MLVM/tooling/mlvm.py")]
    for action, expected in (("check", "ok\n"), ("run", "15\n")):
        assert success(command(launcher + [action, str(ROOT / "MLVM/tests/sum.mlvm"), "--qscript", CLI])) == expected
    output = tmp / "answer.bin"
    success(command(launcher + ["emit-riscv64", str(ROOT / "MLVM/tests/answer.mlvm"), "--qscript", CLI, "-o", str(output)]))
    assert output.read_bytes() == raw

    report = json.loads(success(command(launcher + ["analyze", str(ROOT / "MLVM/tests/sum.mlvm"), "--qscript", CLI])))
    assert report["instruction_count"] == 8 and report["block_count"] == 4 and report["edge_count"] == 8
    assert report["immediate_dominators"] == [0, 0, 1, 2, 3, 4, 5, 5]
    forever = tmp / "forever.mlvm"
    forever.write_text("module 1 { jmp 0; }")
    report = json.loads(success(command(launcher + ["analyze", str(forever), "--qscript", CLI])))
    assert report["immediate_dominators"] == [0] and report["liveness"] == [[0, 0]]
    invalid = tmp / "invalid.mlvm"
    invalid.write_text("module 1 { ret r0; }")
    rejected = command(launcher + ["analyze", str(invalid), "--qscript", CLI])
    assert rejected.returncode == 1 and "undefined register" in rejected.stderr
    rejected = command(launcher + ["run", str(forever), "--optimize", "--global-optimize", "--qscript", CLI])
    assert rejected.returncode == 2
    for jit in (False, True):
        flags = ["--jit"] if jit else []
        assert success(command(launcher + ["run", str(ROOT / "MLVM/tests/sum.mlvm"), "--qscript", CLI,
                                           "--global-optimize"] + flags)) == "15\n"
    optimized = tmp / "diamond.mlvm"
    success(command(launcher + ["opt", str(ROOT / "MLVM/tests/diamond.mlvm"), "--qscript", CLI,
                               "--global-optimize", "-o", str(optimized)]))
    assert "const r2 42;" in optimized.read_text()

print("MLVM core, analyses, global optimization, allocation, CLI and VM/JIT/C/native lowering passed")
