"""Black-box CLI, native/interpreter parity, and GNU ELF interoperability."""
import platform
import random
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

cli = str(Path(sys.argv[1]).resolve())


def invoke(command, source, output=None, success=True, extra=()):
    args = [cli, command, "-"]
    if output:
        args += ["-o", str(output)]
    args += list(extra)
    p = subprocess.run(args, input=source.encode(), capture_output=True, timeout=30)
    assert (p.returncode == 0) == success, (args, p.stdout, p.stderr)
    return p


with tempfile.TemporaryDirectory(prefix="metachain-") as directory:
    root = Path(directory)
    encoded = root / "program.mcil"
    invoke("encode", "const r0, 42\nhalt r0\n", encoded)
    decoded = subprocess.run([cli, "decode", str(encoded)], capture_output=True, check=True)
    assert b"const r0, 42" in decoded.stdout
    invoke("run", "jmp unknown", success=False)
    invoke("rewrite", "add(1, 2)\n", success=False)
    invoke("rewrite", "a: $x => b\nb: $x => a\n%%\na\n", success=False)
    invoke("rewrite", "zero: add($x, 0) => $x\n%%\nadd(add(7, 0), 0)\n", success=True)
    invoke("parse", "rule #S = @MISSING\n%%\nx\n", success=False)
    bad = root / "absent.o"
    assert subprocess.run([cli, "link", str(bad), "-o", str(root / "out")],
                          capture_output=True).returncode == 1
    assert subprocess.run([cli, "link", str(encoded)], capture_output=True).returncode == 1
    assert subprocess.run([cli, "run", "-", "--max-steps", "0"], input=b"const r0, 1\nhalt r0\n",
                          capture_output=True).returncode == 1
    assert subprocess.run([cli, "bogus"], capture_output=True).returncode == 1
    assert subprocess.run([cli], capture_output=True).returncode == 2
    assert subprocess.run([cli, "--help"], capture_output=True).returncode == 0
    for command in ("assemble", "object", "build"):
        invoke(command, ".byte 256" if command != "build" else "nonsense", root / command, success=False)

    if sys.platform == "linux" and platform.machine() in ("x86_64", "AMD64"):
        cases = [
            ("const r0, 42\nprint r0\nhalt r0\n", 42, "42\n"),
            ("const r0, -9223372036854775808\nprint r0\nconst r0, 0\nhalt r0\n", 0, "-9223372036854775808\n"),
            ("const r0, 0\nprint r0\nhalt r0\n", 0, "0\n"),
            ("const r0, 1\nconst r1, 5\nconst r2, 1\nloop: mul r0, r0, r1\nsub r1, r1, r2\njnz r1, loop\nprint r0\nhalt r0\n", 120, "120\n"),
            ("call f\nprint r0\nhalt r0\nf: const r0, 19\nret\n", 19, "19\n"),
            ("const r0, -17\nconst r1, 5\ndiv r2, r0, r1\nmod r3, r0, r1\nprint r2\nprint r3\nhalt r1\n", 5, "-3\n-2\n"),
            ("const r0, -1\nconst r1, 63\nshr r2, r0, r1\nprint r2\nlt r3, r0, r1\neq r4, r0, r0\nprint r3\nprint r4\nhalt r2\n", 1, "1\n1\n1\n"),
            ("const r0, 10\npush r0\nconst r0, 0\npop r1\nprint r1\nhalt r1\n", 10, "10\n"),
        ]
        for n, (source, code, text) in enumerate(cases):
            exe = root / f"native{n}"
            invoke("build", source, exe, extra=("--no-optimize",))
            exe.chmod(0o700)
            p = subprocess.run([str(exe)], capture_output=True, timeout=10)
            assert p.returncode == code and p.stdout.decode() == text, (n, p.returncode, p.stdout, p.stderr)
            interpreted = invoke("run", source)
            assert interpreted.stdout.decode().startswith(text)
        rng = random.Random(1729)
        source = ""
        for op in ("add", "sub", "mul", "div", "mod", "and", "or", "xor", "shl", "shr", "eq", "lt"):
            for _ in range(16):
                a = rng.randrange(-(2**63), 2**63)
                b = rng.randrange(0, 64) if op in ("shl", "shr") else rng.randrange(-100000, 100000) or 1
                source += f"const r0, {a}\nconst r1, {b}\n{op} r2, r0, r1\nprint r2\n"
        source += "const r0, 0\nhalt r0\n"
        expected = invoke("run", source, extra=("--no-optimize",)).stdout.decode().removesuffix("result 0\n")
        exe = root / "differential"
        invoke("build", source, exe, extra=("--no-optimize",))
        exe.chmod(0o700)
        p = subprocess.run([str(exe)], capture_output=True, timeout=10)
        assert p.returncode == 0 and p.stdout.decode() == expected
        for source, code in (("pop r0\nhalt r0\n", 125),
                             ("const r0, 1\nconst r1, 0\ndiv r0, r0, r1\nhalt r0\n", 126),
                             ("loop: jmp loop\n", 124)):
            exe = root / "trap"
            invoke("build", source, exe, extra=("--no-optimize", "--max-steps", "10"))
            exe.chmod(0o700)
            assert subprocess.run([str(exe)], timeout=10).returncode == code
        if shutil.which("as"):
            assembly = root / "gnu.s"
            obj = root / "gnu.o"
            exe = root / "gnu-linked"
            assembly.write_text(".global _start\n.text\n_start:\nmov $60, %rax\nmov $23, %rdi\nsyscall\n")
            subprocess.run(["as", "--64", "-o", str(obj), str(assembly)], check=True)
            subprocess.run([cli, "link", str(obj), "-o", str(exe)], check=True)
            exe.chmod(0o700)
            assert subprocess.run([str(exe)], check=False).returncode == 23
            if shutil.which("objcopy"):
                forms = "\n".join(("mov r8, 81985529216486895", "mov r9, r8", "mov r9d, r10d", "mov spl, cl",
                    "mov r8b, dil", "mov r9b, 255", "add r10, r11", "sub r8d, -305419896", "xor rax, rax",
                    "cmp r14, r15", "test rcx, rdx", "imul r9, r10", "shl r8, 17", "shr rax, cl",
                    "sar r9d, 2", "inc r15", "dec r10d", "neg r11", "not r9d", "idiv r9", "div rcx",
                    "push r15", "pop r9", "jmp r10", "call r11", "lea r9, [r12+305419896]",
                    "mov rax, [r13+305419896]", "mov [r15-305419896], r14", "mov [r12+305419896], spl"))
                assembly.write_text(".intel_syntax noprefix\n.text\n" + forms + "\n")
                subprocess.run(["as", "--64", "-o", str(obj), str(assembly)], check=True)
                reference = root / "reference.bin"
                subprocess.run(["objcopy", "-O", "binary", "-j", ".text", str(obj), str(reference)], check=True)
                ours = root / "ours.bin"
                invoke("assemble", forms, ours)
                assert ours.read_bytes() == reference.read_bytes(), (ours.read_bytes().hex(), reference.read_bytes().hex())
        if shutil.which("ld"):
            obj = root / "ours.o"
            exe = root / "system-linked"
            invoke("object", ".global _start\n_start: mov rax, 60\nmov rdi, 29\nsyscall\n", obj)
            subprocess.run(["ld", "-o", str(exe), str(obj)], check=True)
            assert subprocess.run([str(exe)], check=False).returncode == 29
print("integration checks passed")
