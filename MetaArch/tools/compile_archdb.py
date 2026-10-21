#!/usr/bin/env python3
"""Generate and verify MetaArchDB using MetaArch's own schema reader/DSL loader."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_.-]*\Z")
DECLARATIONS = {
    "targets": "target", "registers": "register", "features": "feature",
    "mnemonics": "mnemonic", "encoders": "encoder", "forms": "form",
    "syntaxes": "syntax", "sections": "section", "relocs": "reloc",
}


def field_name(name):
    if not IDENTIFIER.fullmatch(name):
        raise ValueError(f"DSL cannot represent field name {name!r}")
    return name


def value_text(value, depth=0):
    if isinstance(value, dict):
        entries = [f"{field_name(k)} = {value_text(v, depth + 1)};"
                   for k, v in value.items()]
        flat = "{ " + " ".join(entries) + " }"
        if "\n" not in flat and len(flat) + depth * 4 <= 100:
            return flat
        indent = "    " * (depth + 1)
        return "{\n" + "\n".join(indent + e for e in entries) + "\n" + "    " * depth + "}"
    if isinstance(value, list):
        entries = [value_text(v, depth + 1) for v in value]
        flat = "[" + ", ".join(entries) + "]"
        if "\n" not in flat and len(flat) + depth * 4 <= 100:
            return flat
        indent = "    " * (depth + 1)
        return "[\n" + ",\n".join(indent + e for e in entries) + "\n" + "    " * depth + "]"
    if isinstance(value, float):
        raise ValueError("The DSL does not support floating-point literals")
    if isinstance(value, int) and not -(1 << 63) <= value < (1 << 63):
        raise ValueError(f"Integer outside the DSL's signed 64-bit range: {value}")
    return json.dumps(value, ensure_ascii=True)


def document_text(model, source):
    lines = [f"# Generated from ArchDB/{source.name} by tools/compile_archdb.py.",
             "# Regenerate from the YAML source; see MetaArchDB/README.md.",
             "architecture {"]
    for key, value in model.items():
        declaration = DECLARATIONS.get(key)
        unnamed = key in ("targets", "forms")
        # Names such as '.text' cannot appear in declaration headers. Keep the
        # entire list in assignment syntax: the DSL forbids mixing both styles.
        if (declaration and isinstance(value, list) and value
                and all(isinstance(record, dict) and
                        (unnamed or IDENTIFIER.fullmatch(record.get("name", "")))
                        for record in value)):
            for record in value:
                body = dict(record)
                header = declaration if unnamed else declaration + " " + body.pop("name")
                lines.append("    " + header + " " + value_text(body, 1))
        else:
            lines.append(f"    {field_name(key)} = {value_text(value, 1)};")
    lines.append("}")
    return "\n".join(lines) + "\n"


def read_model(tool, path):
    result = subprocess.run([str(tool), "convert", str(path), "-f", "json"],
                            capture_output=True, text=True, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"Cannot load {path}:\n{result.stderr}")
    return json.loads(result.stdout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", type=Path, default=ROOT / "build/metaarch")
    parser.add_argument("--check", action="store_true",
                        help="check generated files and semantic equivalence without rewriting")
    args = parser.parse_args()
    tool = args.tool.resolve()
    sources = sorted((ROOT / "ArchDB").glob("*.dsymspec.yaml"))
    if not sources:
        raise RuntimeError("No ArchDB specifications found")
    for source in sources:
        arch = source.name.removesuffix(".dsymspec.yaml")
        output = ROOT / "MetaArchDB" / arch / f"{arch}.march"
        model = read_model(tool, source)
        backend = model.get("backend", {})
        if "file" in backend:
            target = (source.parent / backend["file"]).resolve(strict=True)
            link = output.parent / backend["file"]
            relative_target = Path(os.path.relpath(target, link.parent))
            if args.check or link.is_symlink() or link.exists():
                if not link.is_symlink() or link.readlink() != relative_target:
                    raise RuntimeError(f"Expected backend symlink {link} -> {relative_target}")
                if link.resolve(strict=True) != target:
                    raise RuntimeError(f"Backend symlink does not resolve to {target}")
            else:
                link.parent.mkdir(parents=True, exist_ok=True)
                link.symlink_to(relative_target)
        text = document_text(model, source)
        if args.check:
            if output.read_text(encoding="utf-8") != text:
                raise RuntimeError(f"Stale generated DSL: {output}")
        else:
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(text, encoding="utf-8")
        actual = read_model(tool, output)
        if actual != model:
            raise RuntimeError(f"DSL round-trip changed the architecture model: {output}")
        print(f"Verified {output.relative_to(ROOT)}")
    print(f"{len(sources)} architectures verified against their YAML models.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError) as error:
        sys.exit(str(error))
