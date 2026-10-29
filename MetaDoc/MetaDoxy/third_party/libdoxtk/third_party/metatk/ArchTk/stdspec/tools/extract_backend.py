#!/usr/bin/env python3
"""Extract resolved LLVM backend records and supplementary SLEIGH semantics.

Uses llvm-tblgen, not regex parsing of TableGen declarations. The original
record IDs and structured DAGs are retained so consumers can resolve ties,
classes, feature predicates, selection patterns and CPU scheduling variants.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


TARGETS = {
    "aarch64": "AArch64", "amd64": "X86", "x86": "X86",
    "amdgpu": "AMDGPU", "arm32": "ARM", "avr": "AVR", "bpf": "BPF",
    "directx": "DirectX", "hexagon": "Hexagon", "loongarch32": "LoongArch",
    "loongarch64": "LoongArch", "m68k": "M68k", "mips32": "Mips",
    "mips64": "Mips", "nvptx": "NVPTX", "ppc32": "PowerPC",
    "ppc64": "PowerPC", "riscv32": "RISCV", "riscv64": "RISCV",
    "s390x": "SystemZ", "sparc64": "Sparc", "spirv": "SPIRV",
    "wasm": "WebAssembly", "msp430": "MSP430", "lanai": "Lanai",
    "xcore": "XCore",
}
SLEIGH = {
    "msp430": "TI_MSP430/data/languages/TI_MSP430.slaspec",
    "tricore": "tricore/data/languages/tricore.slaspec",
    "avr": "Atmel/data/languages/avr8.slaspec",
    "aarch64": "AARCH64/data/languages/AARCH64.slaspec",
    "riscv64": "RISCV/data/languages/riscv.lp64d.slaspec",
    "riscv32": "RISCV/data/languages/riscv.ilp32d.slaspec",
}
SCHEDULE_MODELS = {
    "X86": {"SkylakeClientModel", "Znver4Model"},
    "AArch64": {"CortexA55Model", "NeoverseN2Model"},
    "ARM": {"CortexA9Model", "CortexM7Model"},
    "AMDGPU": {"GFX10SpeedModel", "GFX11SpeedModel"},
    "Hexagon": {"HexagonModelV60", "HexagonModelV73"},
    "Mips": {"MipsGenericModel", "MipsP5600Model"},
    "RISCV": {"RocketModel", "GenericOOOModel"},
    "PowerPC": {"PPC440Model", "P9Model"},
}
GROUPS = {
    "register_allocation": (
        "Register", "RegisterClass", "RegisterTuples", "SubRegIndex",
        "RegInfo", "RegInfoByHwMode", "RegisterBank", "CalleeSavedRegs",
        "CallingConv", "CCAction",
    ),
    "instruction_selection": (
        "Operand", "RegisterOperand", "Predicate", "ComplexPattern",
        "PatFrag", "Pat", "SDNodeXForm", "SubtargetFeature",
    ),
    "instruction_scheduling": (
        "SchedMachineModel", "Processor", "ProcessorModel", "ProcResourceKind",
        "SchedReadWrite", "ProcWriteResources", "ReadAdvance", "InstRW",
        "ItinRW", "SchedAlias", "InstrItinData", "InstrStage",
        "ProcessorItineraries", "WriteSequence", "SchedVariant",
    ),
}
INSTRUCTION_FIELDS = (
    "AsmString", "Namespace", "Size", "Inst", "OutOperandList", "InOperandList",
    "Constraints", "DisableEncoding", "Defs", "Uses", "Pattern", "Predicates",
    "AddedComplexity", "SchedRW", "Itinerary", "mayLoad", "mayStore",
    "hasSideEffects", "hasDelaySlot", "hasCtrlDep", "isBarrier", "isBranch",
    "isIndirectBranch", "isCall", "isReturn", "isTerminator", "isPseudo",
    "isCodeGenOnly", "isCommutable", "isReMaterializable", "isPredicable",
    "isNotDuplicable", "isConvergent", "mayRaiseFPException",
    "hasExtraDefRegAllocReq", "hasExtraSrcRegAllocReq", "usesCustomInserter",
    "isConvertibleToThreeAddress", "isAsCheapAsAMove", "hasNoSchedulingInfo",
)


def refs(value):
    if isinstance(value, dict):
        if value.get("kind") == "def":
            yield value["def"]
        for item in value.values():
            yield from refs(item)
    elif isinstance(value, list):
        for item in value:
            yield from refs(item)


def record(value, fields=None):
    keys = fields if fields is not None else value.keys()
    out = {k: value[k] for k in keys if k in value and not k.startswith("!")}
    out["source"] = value.get("!locs", [])
    out["classes"] = value.get("!superclasses", [])
    return out


def extract(data, mnemonics, all_instructions=False, models=None):
    instances = data["!instanceof"]
    result = {}
    selected = set()
    for group, classes in GROUPS.items():
        names = {n for c in classes for n in instances.get(c, [])}
        if group == "instruction_selection":
            # Patterns and fragments are added below/from the dependency closure,
            # so unrelated instruction families do not expand the export.
            names -= {n for c in ("Pat", "PatFrag", "SDNodeXForm")
                      for n in instances.get(c, [])}
        if group == "instruction_scheduling" and models:
            names = {n for n in names
                     if ("SchedMachineModel" not in data[n]["!superclasses"] or n in models)
                     and (not data[n].get("SchedModel") or
                          data[n]["SchedModel"].get("def") in models)
                     and not any(c in data[n]["!superclasses"] for c in
                                 ("InstrItinData", "InstrStage", "ProcessorItineraries"))}
        result[group] = {n: record(data[n]) for n in sorted(names)}
        selected.update(names)
    instructions = {}
    counts = {}
    # Retain every matching form, including pseudos with real selection patterns.
    for name in instances.get("Instruction", []):
        item = data[name]
        asm = item.get("AsmString", "")
        match = re.match(r"[a-zA-Z0-9_.]+", asm.strip())
        mnemonic = match[0].lower() if match else ""
        if (all_instructions and item.get("Namespace") not in ("", "TargetOpcode")) or mnemonic in mnemonics:
            instructions[name] = record(item, INSTRUCTION_FIELDS)
            counts[mnemonic] = counts.get(mnemonic, 0) + 1
    result["instructions"] = dict(sorted(instructions.items()))
    for name in instances.get("Pat", []):
        if set(refs(data[name].get("ResultInstrs"))) & instructions.keys():
            result["instruction_selection"][name] = record(data[name])
            selected.add(name)
    selected.update(instructions)
    # Definitions referenced by any exported record are a transitive closure;
    # this includes anonymous CC rules, DAG operators, types and sched predicates.
    pending = set(refs(result)) - selected
    dependencies = {}
    while pending:
        name = pending.pop()
        if name in selected or name not in data:
            continue
        selected.add(name)
        dependencies[name] = record(data[name])
        pending.update(set(refs(dependencies[name])) - selected)
    result["dependencies"] = dict(sorted(dependencies.items()))
    result["coverage"] = {
        "instruction_scope": ("all target instructions" if all_instructions else
                              "all TableGen forms whose assembly mnemonic occurs in this spec"),
        "matched_mnemonics": counts,
        "unmatched_mnemonics": sorted(mnemonics - counts.keys()),
        "mode_filtering": "consumer must apply predicates, ABI and subtarget mode",
        "unknown_values": "null is unresolved, not false or zero",
        "register_members": "MemberList is a set-expression DAG; not a reserved-register mask",
        "scheduling": "select a CPU model; ISA-wide latency is not defined",
        "selected_schedule_models": sorted(models) if models else "all available models",
    }
    return result


def spec_mnemonics(path):
    # Execute declaration loops, rather than guessing their shape with regex.
    collector = '''
local names = {}
local builder = setmetatable({}, {__index = function(_, k)
  if k == "mnemonic" then return function(t) names[t.name] = true end end
  return function() return "" end
end})
local module = {new = function() return builder end}
package.preload["backend"] = function() return module end
package.preload["dsl"] = function() return module end
local output = print
print = function() end
dofile(arg[1])
for name in pairs(names) do output(name) end
'''
    return {name.lower() for name in subprocess.check_output(
        ["lua", "-", str(path)], input=collector, text=True).splitlines()}


def sleigh_metadata(root, relative, mnemonics):
    import domsleigh
    spec = domsleigh.to_data(domsleigh.load_slaspec(root / relative))
    constructors = spec.get("constructors", [])
    # Parser output is supplementary semantics, not a validated encoding table.
    result = {
        "source": relative, "parser_version": domsleigh.__version__,
        "source_sha256": hashlib.sha256((root / relative).read_bytes()).hexdigest(),
        "preprocessed_sha256": hashlib.sha256(domsleigh.preprocess_file(root / relative).encode()).hexdigest(),
        "endian": spec.get("endian"), "alignment": spec.get("alignment"),
        "spaces": spec.get("spaces", []), "tokens": spec.get("tokens", []),
        "constructors": constructors,
        "macros": spec.get("macros", []),
        "contexts": spec.get("contexts", []),
        "attachments": spec.get("attachments", []),
        "limitations": "DomSleigh AST only; do not infer register overlap from varnode offsets or timing from p-code",
    }
    if relative.startswith("tricore/"):
        result["compiler_specs"] = {
            p.name: domsleigh.to_data(domsleigh.load_cspec(p))
            for p in sorted((root / relative).parent.glob("*.cspec"))}
    return result


def serialize_metadata(metadata):
    # One resolved record per line keeps generated banks reviewable without
    # expanding deeply nested DAGs into hundreds of mostly whitespace lines.
    fields = []
    record_maps = {*GROUPS, "instructions", "dependencies"}
    for key, value in sorted(metadata.items()):
        if key in record_maps:
            rows = ["    " + json.dumps(n) + ": " +
                    json.dumps(r, sort_keys=True, separators=(",", ":"))
                    for n, r in sorted(value.items())]
            body = "{\n" + ",\n".join(rows) + "\n  }"
        else:
            body = json.dumps(value, sort_keys=True, indent=2)
        fields.append("  " + json.dumps(key) + ": " + body)
    return "{\n" + ",\n".join(fields) + "\n}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--src", type=Path, required=True, help="LLVM-Target directory")
    parser.add_argument("--include", type=Path, required=True, help="LLVM include directory")
    parser.add_argument("--sleigh", type=Path, help="Ghidra-CPU directory")
    parser.add_argument("--out", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--arch", action="append", help="architecture key; default all")
    parser.add_argument("--tblgen", default="llvm-tblgen")
    parser.add_argument("--revision", default="llvmorg-22.1.8", help="source revision recorded in provenance")
    args = parser.parse_args()
    version = subprocess.check_output([args.tblgen, "--version"], text=True).strip()
    policies = json.loads(Path(__file__).with_name("backend_policies.json").read_text())
    args.out.joinpath("backend").mkdir(exist_ok=True)
    cache = {}
    for arch in args.arch or [*TARGETS, "tricore"]:
        mnemonics = spec_mnemonics(args.out.joinpath(arch + ".dsymspec"))
        target = TARGETS.get(arch)
        metadata = {"schema": 1, "arch": arch}
        if target:
            if target not in cache:
                entry_name = "PPC" if target == "PowerPC" else target
                entry = args.src / target / (entry_name + ".td")
                with tempfile.TemporaryDirectory(prefix="tblgen-", dir="/tmp/opencode") as tmp:
                    output = Path(tmp) / "records.json"
                    subprocess.run([args.tblgen, "--dump-json", "-I", str(args.include),
                                    "-I", str(entry.parent), str(entry), "-o", str(output)], check=True)
                    cache[target] = json.loads(output.read_text())
            metadata.update(extract(cache[target], mnemonics,
                                    arch in {"msp430", "lanai", "xcore", "spirv", "directx"},
                                    SCHEDULE_MODELS.get(target)))
            metadata["provenance"] = {
                "tool": version, "target": target,
                "sources": {str(p.relative_to(args.src)): hashlib.sha256(p.read_bytes()).hexdigest()
                            for p in sorted((args.src / target).rglob("*.td"))},
                "revision": args.revision,
                "llvm_include_sources": {str(p.relative_to(args.include)): hashlib.sha256(p.read_bytes()).hexdigest()
                                         for p in sorted(args.include.rglob("*.td"))},
                "upstream": "https://github.com/llvm/llvm-project/tree/" + args.revision + "/llvm/lib/Target/" + target,
                "license": "Apache-2.0 WITH LLVM-exception",
            }
        else:
            metadata.update({g: {} for g in GROUPS})
            metadata["instructions"] = {}
            metadata["dependencies"] = {}
            metadata["coverage"] = {"tablegen": "no LLVM target; SLEIGH semantics only"}
        if args.sleigh and arch in SLEIGH:
            metadata["sleigh"] = sleigh_metadata(args.sleigh, SLEIGH[arch], mnemonics)
        policy_key = "riscv" if arch in {"riscv32", "riscv64"} else arch
        if policy_key in policies:
            metadata["compiler_policy"] = policies[policy_key]
        # Lua long strings preserve JSON null, empty arrays and structured DAGs.
        encoded = serialize_metadata(metadata)
        assert "]=]" not in encoded
        args.out.joinpath("backend", arch + ".lua").write_text(
            "-- Generated by tools/extract_backend.py; see README.md.\nreturn [=[" + encoded + "]=]\n")
        print(arch, len(metadata["instructions"]), "instruction records")


if __name__ == "__main__":
    main()
