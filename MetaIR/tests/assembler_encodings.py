"""Compare register/immediate/memory forms with GNU as, not another opcode table."""
import json
import pathlib
import subprocess
import sys
import tempfile

cli, assembler, objcopy = sys.argv[1:]
root = pathlib.Path(__file__).resolve().parent.parent
bank = json.loads((root / "Arch/stdspec/asmtk/amd64.json").read_text())
registers = {(r["enc"], r["width"]): r["name"] for r in bank["registers"]}
source, reference = [], []


def add(ours, theirs=None):
    source.append(ours)
    reference.append(theirs or ours)


for e in bank["asmtk"]["encodings"]:
    name, form = e["mnemonic"], e["form"]
    if form == "Fixed":
        add(name)
    for width in e["widths"]:
        for code in range(16):
            r = registers[code, width]
            if form in ("Indirect", "OpcodeRegister", "Unary"):
                add(f"{name} {r}")
            elif form == "RegisterRegister":
                for other in range(16):
                    add(f"{name} {r}, {registers[other, width]}")
            elif form in ("Load", "Store"):
                for base in range(16):
                    # A large displacement forces disp32 in both assemblers.
                    memory = f"[{registers[base, 64]}+123456]"
                    add(f"{name} {r}, {memory}" if form == "Load" else f"{name} {memory}, {r}")
            elif form == "MoveImmediate":
                value = 123 if width == 8 else 0x12345678 if width == 32 else 0x123456789abcdef0
                add(f"{name} {r}, {value}", f"{'movabs' if width == 64 else name} {r}, {value}")
            elif form == "ShiftCL":
                add(f"{name} {r}, cl")
            elif form == "ShiftImmediate":
                add(f"{name} {r}, 7")
            elif form == "ArithmeticImmediate" and code != 0:
                # A large value avoids imm8. GAS always chooses a shorter,
                # accumulator-special opcode for code 0; golden tests cover it.
                add(f"{name} {r}, 123456")


def run(command, **kwargs):
    result = subprocess.run(command, capture_output=True, timeout=120, **kwargs)
    if result.returncode:
        raise AssertionError(f"{command[0]} failed: {result.stderr}")

with tempfile.TemporaryDirectory(prefix="metachain-assembler-") as directory:
    work = pathlib.Path(directory)
    run([cli, "assemble", "-o", str(work / "ours.bin")], input="\n".join(source) + "\n", text=True)
    run([assembler, "--64", "-o", str(work / "reference.o")],
        input=".intel_syntax noprefix\n" + "\n".join(reference) + "\n", text=True)
    run([objcopy, "-O", "binary", "-j", ".text", str(work / "reference.o"), str(work / "reference.bin")])
    ours, theirs = (work / "ours.bin").read_bytes(), (work / "reference.bin").read_bytes()
    if ours != theirs:
        first = next((i for i, (a, b) in enumerate(zip(ours, theirs)) if a != b), min(len(ours), len(theirs)))
        raise AssertionError(f"encoding mismatch at byte {first}: ASMtk {ours[first:first+16].hex()}, GNU as {theirs[first:first+16].hex()}")
print(f"{len(source)} Arch-backed instructions match GNU as ({len(ours)} bytes)")
