#!/usr/bin/env python3
"""Integration checks with real native assemblers, object readers and linkers."""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess


parser = argparse.ArgumentParser()
parser.add_argument("--tool", required=True)
parser.add_argument("--source", type=Path, required=True)
parser.add_argument("--work", type=Path, required=True)
parser.add_argument("--cc", required=True)
parser.add_argument("--lua", type=int, required=True)
parser.add_argument("--plugin", default="")
parser.add_argument("--bad-plugin", default="")
args = parser.parse_args()
args.work.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, METAARCHDB=str(args.source / "MetaArchDB"),
           ARCHBFDDB=str(args.source / "ArchBFD"))
if Path("/tmp/opencode").is_dir():
    env["TMPDIR"] = "/tmp/opencode"
checks = 0


def run(*options, code=0, source=None):
    global checks
    result = subprocess.run([args.tool, *map(str, options)], input=source,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            cwd=args.work, env=env, timeout=30)
    checks += 1
    assert result.returncode == code, (options, code, result.returncode,
                                        result.stdout, result.stderr)
    return result


def write(name, source):
    path = args.work / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(source)
    return path


def command(*argv, code=0):
    global checks
    result = subprocess.run(list(map(str, argv)), cwd=args.work, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
    checks += 1
    assert result.returncode == code, (argv, result.returncode, result.stdout, result.stderr)
    return result


run("--help")
run("--version")
run(code=2)
run("-T", code=2)
run("--unknown", code=2)
run("--help=bad", code=2)
run("-T", "amd64", "--emit=unknown", "-", code=2)
run("--list-targets", "file.mair", code=2)
run("--list-targets", "--list-forms", code=2)
run("-T", "amd64", "--target-index", "-1", "-", code=2)
run("-T", "amd64", "--max-bytes", "0", "-", code=2)
run("-T", "amd64", "--assembler", "as", "-", code=2)
run("-T", "amd64", "missing.mair", code=3)
run("-T", "missing", "--emit=asm", "-", source=b"encode ret", code=1)
assert b"aarch64\namd64" in run("--list-targets").stdout
assert b"ret" in run("-T", "amd64", "--list-encoders").stdout
assert b"11\taddi\taddi" in run("-T", "riscv64", "--list-forms").stdout

basic = ("-T", "amd64", "--backend", "builtin", "--emit=hex")
assert run(*basic, "-", source=b"encode ret").stdout == b"c3\n"
assert run(*basic, "-DVALUE=17", "-", source=b".byte VALUE").stdout == b"11\n"
run(*basic, "-DVALUE=1", "-DVALUE=2", "-", code=2)
run(*basic, "--max-bytes", "2", "-", source=b"encode ret", code=1)
run(*basic, "--max-depth", "2", "-", source=b".macro m\nm\n.endm\nm", code=1)

ret = write("ret.mair", "encode ret\n")
run(*basic, ret, "-o", ret, code=2)
alias = args.work / "ret-alias.mair"
if alias.exists() or alias.is_symlink():
    alias.unlink()
alias.symlink_to(ret)
run(*basic, ret, "-o", alias, code=2)
alias.unlink()
os.link(ret, alias)
run(*basic, ret, "-o", alias, code=2)
alias.unlink()
run(*basic, "--check", ret)
assert not ret.with_suffix(".hex").exists()
run(*basic, "--check", ret, "-o", "out", code=2)
preserve = write("preserve.o", "preserve existing output")
bad = write("bad.mair", "# comment\n.encode missing\n")
error = run(*basic, bad, "-o", preserve, code=1).stderr
assert b"bad.mair:2:1:" in error
assert preserve.read_text() == "preserve existing output"
run(*basic, ret, "-o", args.work / "nonexistent" / "out.hex", code=3)

child = write("include dir/child.inc", ".macro val n:req\n.byte \\n\n.endm\nval 9\n")
parent = write("include dir/parent.mair", '.include "child.inc"\nval 3\n')
assert run(*basic, parent, "-o", "-").stdout == b"0903\n"
assert run(*basic, "-I", child.parent, "-", source=b'.include "child.inc"').stdout == b"09\n"
run(*basic, parent, "-o", child, code=2)
child.write_text('.include "parent.mair"\n')
assert b"recursive include" in run(*basic, parent, code=1).stderr
child.write_text("# first\n.encode missing\n")
assert b"child.inc:2:1:" in run(*basic, parent, code=1).stderr
run(*basic, "-", source=b'.include "missing.inc"', code=3)

for arch, expected in [("amd64", b"c3\n"), ("aarch64", b"c0035fd6\n"),
                       ("riscv64", b"67800000\n")]:
    assert run("-T", arch, "--backend=builtin", "--emit=hex", "-",
               source=b"encode ret").stdout == expected
assert run("-T", "riscv64", "--backend=builtin", "--emit=hex",
           args.source / "MetaArchIR/examples/cli/riscv64/decrement.mair", "-o", "-").stdout == b"1305f5ff67800000\n"
assert run(*basic, "--db", args.source / "ArchDB", "-", source=b"encode ret").stdout == b"c3\n"
assert run(*basic, "--spec", args.source / "ArchDB/amd64.dsymspec.yaml", "-",
           source=b"encode ret").stdout == b"c3\n"
image = json.loads(run("-T", "amd64", "--emit=json", "-",
                      source=b"encode call, sym=@callee-4\nencode ret").stdout)
assert image["relocations"][0]["symbol"] == "callee"
assert image["relocations"][0]["value"] == -4
assert image["relocations"][0]["offset"] == 1

if args.lua:
    result = run(*basic, "-DCOUNT=3", args.source / "ArchAS/examples/templates.mair", "-o", "-")
    assert result.stdout == b"01000000040000000900000007070707\n"
    assert run(*basic, "--lua", args.source / "ArchAS/examples/directives.lua", "-",
               source=b".u32 0x12345678").stdout == b"78563412\n"
    lua_source = b"{% domas.directive('loop', function() return '.loop' end) %}\n.loop"
    assert b"depth exceeded" in run(*basic, "-", source=lua_source, code=1).stderr
    run(*basic, "-", source=b"{% domas.directive('bad', function() return 42 end) %}\n.bad", code=1)
    run(*basic, "-", source=b"{% domas.directive('same', function() end); domas.directive('same', function() end) %}", code=1)
    run(*basic, "--lua-instructions", "1000", "-", source=b"{% while true do end %}", code=1)
    run(*basic, "--lua-memory", "100", "-", source=b"encode ret", code=1)
    result = run(*basic, "-", source=b"# comment\n{% error('broken') %}", code=1)
    assert b"<stdin>:2:1:" in result.stderr
else:
    assert b"disabled" in run(*basic, "-", source=b"{{1}}", code=1).stderr

if args.plugin:
    assert run(*basic, "--plugin", args.plugin, "-", source=b".pair 13").stdout == b"0d0d\n"
    source = b".macro m x:req\n.pair \\x\n.endm\nm 2\n"
    assert run(*basic, "--plugin", args.plugin, "-", source=source).stdout == b"0202\n"
    assert b"requires an expression" in run(*basic, "--plugin", args.plugin, "-", source=b".pair", code=1).stderr
    assert b"duplicate directive" in run(*basic, "--plugin", args.plugin, "--plugin", args.plugin,
                                         "-", source=b"", code=1).stderr
    assert b"ABI mismatch" in run(*basic, "--plugin", args.bad_plugin, "-", source=b"", code=1).stderr
    run(*basic, "--plugin", args.work / "missing.so", "-", source=b"", code=3)
    if args.lua:
        run(*basic, "--plugin", args.plugin, "-", source=b"{% domas.directive('pair', function() end) %}", code=1)

host_x86 = platform.machine().lower() in ("x86_64", "amd64") and platform.system() == "Linux"
llvm = shutil.which("llvm-mc")
gas = shutil.which("as") if host_x86 else None
readelf = shutil.which("readelf")
objcopy = shutil.which("llvm-objcopy") or shutil.which("objcopy")

if gas or llvm:
    backend = "gas" if gas else "llvm"
    native = ("-T", "amd64", "--backend", backend)
    answer = write("answer.mair", (args.source / "ArchAS/examples/answer.mair").read_text())
    run(*native, "--intel-syntax", answer)
    answer_object = answer.with_suffix(".o")
    assert answer_object.read_bytes().startswith(b"\x7fELF")
    original = answer_object.read_bytes()
    run(*native, "--intel-syntax", answer)
    assert answer_object.read_bytes() == original
    assert run(*native, "--intel-syntax", "--emit=hex", "-", source=b"mov eax,42\nret").stdout == b"b82a000000c3\n"
    assert run(*native, "--emit=hex", "-", source=b"mov $42,%eax\nret").stdout == b"b82a000000c3\n"
    assert run(*native, "--emit=hex", "-", source=b".byte 'A, ';, '#, '\\n\n").stdout == b"413b230a\n"
    assert run(*native, "--emit=hex", "-", source=b"begin:\n.byte 1,2\n.equ len, .-begin\n.byte len\n").stdout == b"010202\n"
    assert run(*native, "--emit=hex", "-", source=b".byte 1\njmp 1f\n.byte 2\n1: ret").stdout in (b"01eb0102c3\n", b"01e90100000002c3\n")
    assert b"bad-native.mair:2:" in run(*native, write("bad-native.mair", "# comment\nnot_an_instruction\n"), code=1).stderr
    native_preserve = run(*native, "-", "-o", preserve, source=b"invalid_opcode", code=1)
    assert preserve.read_text() == "preserve existing output"
    run(*native, "--assembler", args.work / "missing-as", "-", source=b"ret", code=3)
    run(*native, "--emit=bin", "-", source=b"call external\n", code=1)
    run(*native, "--emit=bin", "-", source=b".text\n.byte 1\n.data\n.byte 2", code=1)
    assert run(*native, "--emit=hex", "-", source=b".bss\n.zero 4\n").stdout == b"00000000\n"
    object_path = args.work / "relocation.o"
    run(*native, "-", "-o", object_path, source=b".text\n.globl caller\ncaller:\nencode call, sym=@callee-4\nencode ret\n")
    if readelf:
        relocs = command(readelf, "-Wr", object_path).stdout
        assert b"R_X86_64_PLT32" in relocs and b"callee - 4" in relocs
    if args.plugin:
        assert run(*native, "--plugin", args.plugin, "--emit=hex", "-", source=b".pair 9").stdout == b"0909\n"
    if host_x86:
        driver = write("driver.c", "extern int answer(void); int main(void) { return answer()!=42; }\n")
        command(args.cc, driver, answer_object, "-o", args.work / "answer-test")
        command(args.work / "answer-test")
        # Exercise the actual ELF RELA addend by linking and executing the call.
        driver = write("caller.c", "int callee(void) { return 31; } extern int caller(void); int main(void) { return caller()!=31; }\n")
        command(args.cc, driver, object_path, "-o", args.work / "caller-test")
        command(args.work / "caller-test")
        archld = args.source / "ArchLD/bin/ArchLD"
        if archld.is_file() and os.access(archld, os.X_OK):
            start = args.work / "start.o"
            run(*native, "--intel-syntax", args.source / "ArchAS/examples/start.mair", "-o", start)
            command(archld, "-o", args.work / "linked", start)
            command(args.work / "linked", code=37)
        else:
            print("SKIP: ArchLD executable unavailable")
else:
    print("SKIP: native object tests require GNU as or llvm-mc")

if llvm and objcopy:
    for arch, assembly, expected in [
            ("aarch64", "add x0,x0,x1\nret\n", b"0000018bc0035fd6\n"),
            ("riscv64", "addi a0,a0,-1\nret\n", b"1305f5ff67800000\n"),
            # The shipped ppc64 target is ppc64le/ELFv2, unlike big-endian ppc32.
            ("ppc64", "addi 3,3,-1\nblr\n", b"ffff63382000804e\n"),
            ("ppc32", "addi 3,3,-1\nblr\n", b"3863ffff4e800020\n")]:
        result = run("-T", arch, "--backend=llvm", "--emit=hex", "-", source=assembly.encode())
        assert result.stdout == expected, (arch, result.stdout, expected)
        if arch in ("aarch64", "riscv64"):
            mair = (args.source / "MetaArchIR/examples/cli" / arch /
                    ("add.mair" if arch == "aarch64" else "decrement.mair"))
            actual = args.work / (arch + ".o")
            expected_object = args.work / (arch + "-reference.o")
            native_source = write(arch + ".s", assembly)
            run("-T", arch, "--backend=llvm", mair, "-o", actual)
            command(llvm, "-triple=" + arch, "-filetype=obj", native_source, "-o", expected_object)
            for obj in (actual, expected_object):
                command(objcopy, "-O", "binary", "--only-section=.text", obj, str(obj) + ".bin")
            assert Path(str(actual) + ".bin").read_bytes() == Path(str(expected_object) + ".bin").read_bytes()
    # amd64's checked encoder and LLVM's native path agree on the call relocation.
    reloc = args.work / "llvm-relocation.o"
    run("-T", "amd64", "--backend=llvm", "-", "-o", reloc,
        source=b"encode call, sym=@callee-4\nencode ret")
    if readelf:
        assert b"callee - 4" in command(readelf, "-Wr", reloc).stdout
else:
    print("SKIP: cross-assembler comparisons require LLVM tools")

print(f"{checks} CLI/tool invocations passed")
