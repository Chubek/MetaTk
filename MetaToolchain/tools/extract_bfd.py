#!/usr/bin/env python3
"""Generate the ArchBFD object-format descriptions from a binutils checkout.

The ArchBFD catalogue (ArchBFD/*.bfd.yaml) describes the object formats that
BFD (binutils/bfd) implements: the container layouts, the numeric constants
and the relocation ("howto") tables.  All of that data exists in the binutils
sources; this script extracts it mechanically so the committed YAML stays
verifiable instead of hand-transcribed.

Usage:

    python3 tools/extract_bfd.py [--binutils DIR] [--out DIR] [--check]

    --binutils DIR   binutils checkout to read (default: ./binutils)
    --out DIR        directory to write (default: ./ArchBFD)
    --check          do not write; fail when the committed files differ

The output is deterministic: running the script twice yields byte-identical
files.  Each generated file carries a header comment naming the sources it
was derived from; edit the script, not the YAML.
"""

import argparse
import os
import re
import sys

# --------------------------------------------------------------------------
# C source helpers
# --------------------------------------------------------------------------

_COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)


def strip_comments(text):
    """Remove C comments, keeping the text they replaced (one space)."""
    return _COMMENT_RE.sub(" ", text)


def read_text(root, relpath):
    with open(os.path.join(root, relpath), "r", encoding="utf-8", errors="replace") as fh:
        return fh.read()


def find_block(text, start_pat):
    """Return the body between the first '{' after start_pat and its match."""
    m = re.search(start_pat, text)
    if not m:
        raise LookupError("pattern not found: %s" % start_pat)
    i = text.index("{", m.end() - 1 if text[m.end() - 1 : m.end()] == "{" else m.end())
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i + 1 : j]
    raise LookupError("unterminated block for %s" % start_pat)


def find_struct_body(text, struct_name):
    """Return the body of `struct <name>` or of an anonymous typedef.

    The binutils headers use both spellings:
        struct external_filehdr { ... };
        typedef struct { ... } Elf64_External_Ehdr;
    """
    m = re.search(r"\b(?:struct|union)\s+%s\b" % re.escape(struct_name), text)
    if m:
        i = text.index("{", m.end())
        depth = 0
        for j in range(i, len(text)):
            if text[j] == "{":
                depth += 1
            elif text[j] == "}":
                depth -= 1
                if depth == 0:
                    return text[i + 1 : j]
        raise LookupError("unterminated struct %s" % struct_name)
    m = re.search(r"\}\s*%s\s*;" % re.escape(struct_name), text)
    if m:
        close = m.start()
        depth = 0
        for j in range(close, -1, -1):
            if text[j] == "}":
                depth += 1
            elif text[j] == "{":
                depth -= 1
                if depth == 0:
                    return text[j + 1 : close]
        raise LookupError("unterminated typedef %s" % struct_name)
    raise LookupError("struct not found: %s" % struct_name)


def split_top_level(text, sep=","):
    """Split on `sep` at paren/brace/bracket depth zero."""
    out, depth, cur = [], 0, []
    for ch in text:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == sep and depth == 0:
            out.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    tail = "".join(cur).strip()
    if tail:
        out.append(tail)
    return out


_FIELD_RE = re.compile(
    r"^\s*(?:const\s+|unsigned\s+|signed\s+|struct\s+|union\s+|enum\s+)*"
    r"[A-Za-z_][A-Za-z_0-9]*\s*"
    r"(?P<name>[A-Za-z_][A-Za-z_0-9]*)\s*"
    r"(?P<dims>(?:\[[^\]]*\])*)\s*;",
    re.M,
)


def _dims_size(dims, defines=None):
    total = 1
    for d in re.findall(r"\[([^\]]*)\]", dims):
        d = d.strip()
        if not d:
            continue
        try:
            total *= int(d, 0)
        except ValueError:
            value = (defines or {}).get(d)
            if value is None:
                total *= 0  # unknown dimension (e.g. sizeof expression)
            else:
                total *= value
    return total


def parse_struct_fields(text, struct_name):
    """Return [(field, size, comment)] for a struct in `text`.

    Anonymous unions/structs are flattened to their first member, which is how
    the COFF symbol entry stores its name.
    """
    return parse_struct_fields_with_comments(text, struct_name)


def _match_brace(text, start):
    """Index of the '}' matching the '{' at `start`."""
    depth = 0
    for k in range(start, len(text)):
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                return k
    raise LookupError("unbalanced braces")


def _mask_comments(text):
    """Replace every comment with spaces, keeping the offsets intact.

    The structural scan must not see a ';' or a '{' that lives inside a
    comment, and the masked text keeps the same offsets as the original so
    that the descriptions can still be looked up.
    """
    def repl(m):
        return "".join("\n" if c == "\n" else " " for c in m.group(0))

    return _COMMENT_RE.sub(repl, text)


def _scan_members(body, fields, defines=None, original=None):
    """Collect (name, size, comment) tuples from a struct body.

    Nested unions contribute their first member only (the COFF symbol entry
    stores either a name or a string-table offset in the same space); nested
    structs are flattened.
    """
    masked = _mask_comments(body)
    src = original if original is not None else masked
    i = 0
    n = len(masked)
    while i < n:
        brace = masked.find("{", i)
        semi = masked.find(";", i)
        if brace >= 0 and (semi < 0 or brace < semi):
            end = _match_brace(masked, brace)
            inner = masked[brace + 1 : end]
            tail = masked[end + 1 :]
            m = re.match(r"\s*([A-Za-z_][A-Za-z_0-9]*)?\s*(\[[^\]]*\])*\s*", tail)
            name = m.group(1) if m else None
            keyword = _block_keyword(masked, brace)
            if keyword == "union" or name is None:
                sub = []
                _scan_members(inner, sub, defines, src[brace + 1 : end])
                if sub:
                    fields.append(sub[0])
            else:
                _scan_members(inner, fields, defines, src[brace + 1 : end])
            i = end + 1
            continue
        if semi < 0:
            break
        chunk = masked[i : semi + 1]
        m = _FIELD_RE.match(chunk)
        if m:
            size = _dims_size(m.group("dims"), defines)
            if size:
                rest = src[semi + 1 :].split("\n", 1)[0]
                cm = re.search(r"/\*(.*?)\*/", rest)
                desc = " ".join(cm.group(1).split()) if cm else ""
                fields.append((m.group("name"), size, desc))
        i = semi + 1
    return fields


def _block_keyword(body, brace):
    """The keyword ('struct' or 'union') introducing the block at `brace`."""
    head = strip_comments(body[:brace]).rstrip()
    m = re.search(r"([A-Za-z_][A-Za-z_0-9]*)\s*$", head)
    return m.group(1) if m else ""


def parse_struct_fields_with_comments(text, struct_name, defines=None):
    """Return [(field, size, comment)] for a struct in `text`."""
    return _scan_members(find_struct_body(text, struct_name), [], defines)


_DEFINE_RE = re.compile(r"^#define\s+([A-Za-z_][A-Za-z_0-9]*)\s+(\S+)\s*(?:/\*(.*?)\*/)?\s*$", re.M)


def parse_defines(text):
    """Return {name: (value_text, comment)} for simple #define constants."""
    out = {}
    for m in _DEFINE_RE.finditer(text):
        name, value, comment = m.group(1), m.group(2), m.group(3) or ""
        out[name] = (value, " ".join(comment.split()))
    return out


_RELOC_NUMBER_RE = re.compile(r"RELOC_NUMBER\s*\(\s*([A-Za-z_][A-Za-z_0-9]*)\s*,\s*([0-9]+)\s*\)")


def parse_reloc_numbers(text):
    """Return {name: number} for the RELOC_NUMBER() enum used by ELF headers."""
    return {m.group(1): int(m.group(2)) for m in _RELOC_NUMBER_RE.finditer(text)}


_ENUM_RE = re.compile(r"typedef\s+enum[^{]*\{(.*?)\}\s*[A-Za-z_][A-Za-z_0-9]*\s*;", re.S)
_ENUM_ITEM_RE = re.compile(r"([A-Za-z_][A-Za-z_0-9]*)\s*=\s*"
                           r"(0[xX][0-9a-fA-F]+|[0-9]+)")


def parse_enums(text):
    """Return {name: value} for the enumerators of every enum in `text`."""
    out = {}
    for m in _ENUM_RE.finditer(text):
        body = strip_comments(m.group(1))
        for item in _ENUM_ITEM_RE.finditer(body):
            out[item.group(1)] = int(item.group(2), 0)
    return out


# --------------------------------------------------------------------------
# howto (relocation) tables
# --------------------------------------------------------------------------

_SIZE_CODES = {0: 1, 1: 2, 2: 4, 3: 0, 4: 8, 8: 16, -2: 4}

_OVERFLOW_NAMES = {
    "complain_overflow_dont": "dont",
    "complain_overflow_bitfield": "bitfield",
    "complain_overflow_signed": "signed",
    "complain_overflow_unsigned": "unsigned",
    "complain_overflow_truncate": "truncate",
    "complain_overflow_error": "error",
}

_TRUE_FALSE = {"TRUE": True, "FALSE": False, "true": True, "false": False}


def _num(token):
    token = token.strip()
    if token in _TRUE_FALSE:
        return int(_TRUE_FALSE[token])
    if token == "MINUS_ONE":
        return (1 << 64) - 1
    if token.startswith("0x") or token.startswith("0X"):
        return int(token, 16)
    try:
        return int(token, 0)
    except ValueError:
        return None


def parse_howto_table(text, table_name):
    """Extract a BFD reloc_howto_type table.

    Handles the HOWTO()/EMPTY_HOWTO() macros and the raw brace initialisers
    used by bfd/elf-hppa.h.  Returns a list of dicts with the BFD field names.
    """
    body = find_block(text, r"\breloc_howto_type\s+%s\s*\[" % re.escape(table_name))
    body = strip_comments(body)
    entries = []
    for raw in split_top_level(body):
        item = raw.strip()
        if not item:
            continue
        if item.startswith("EMPTY_HOWTO"):
            args = split_top_level(item[item.index("(") + 1 : item.rindex(")")])
            entries.append(
                {
                    "type": args[0].strip(),
                    "rightshift": 0,
                    "size": 0,
                    "bitsize": 0,
                    "pc_relative": False,
                    "bitpos": 0,
                    "overflow": "complain_overflow_dont",
                    "name": None,
                    "partial_inplace": False,
                    "src_mask": 0,
                    "dst_mask": 0,
                    "pcrel_offset": False,
                    "empty": True,
                }
            )
            continue
        if item.startswith("HOWTO"):
            args = split_top_level(item[item.index("(") + 1 : item.rindex(")")])
        elif item.startswith("{"):
            args = split_top_level(item[1 : item.rindex("}")])
        else:
            continue
        if len(args) < 13:
            raise ValueError("short howto entry in %s: %r" % (table_name, item))
        name = None
        if args[8].strip().startswith('"'):
            name = args[8].strip().strip('"')
        entries.append(
            {
                "type": args[0].strip(),
                "rightshift": _num(args[1]),
                "size": _num(args[2]),
                "bitsize": _num(args[3]),
                "pc_relative": _num(args[4]),
                "bitpos": _num(args[5]),
                "overflow": args[6].strip(),
                "name": name,
                "partial_inplace": _num(args[9]),
                "src_mask": _num(args[10]),
                "dst_mask": _num(args[11]),
                "pcrel_offset": _num(args[12]),
                "empty": False,
            }
        )
    return entries


# --------------------------------------------------------------------------
# YAML emission
# --------------------------------------------------------------------------

_PLAIN_SAFE = re.compile(r"^[A-Za-z_][A-Za-z_0-9_.-]*$")
_RESERVED = {
    "y", "Y", "yes", "Yes", "YES", "n", "N", "no", "No", "NO",
    "true", "True", "TRUE", "false", "False", "FALSE",
    "on", "On", "ON", "off", "Off", "OFF", "null", "Null", "NULL", "~",
}


def yaml_scalar(value):
    if value is None:
        return "null"
    if value is True:
        return "true"
    if value is False:
        return "false"
    if isinstance(value, int):
        return str(value)
    text = str(value)
    if text == "":
        return '""'
    if _PLAIN_SAFE.match(text) and text not in _RESERVED:
        return text
    escaped = (
        text.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\t", "\\t")
        .replace("\r", "\\r")
    )
    return '"%s"' % escaped


def _is_scalar(value):
    return value is None or isinstance(value, (bool, int, str))


def _flow_mapping(mapping):
    return "{" + ", ".join("%s: %s" % (yaml_scalar(k), yaml_scalar(v)) for k, v in mapping.items()) + "}"


def emit_yaml(value, indent=0):
    """Render `value` as YAML text (block style, flow style for leaf records)."""
    lines = []
    _emit(value, indent, lines)
    return "\n".join(lines) + "\n"


def _emit(value, indent, lines):
    pad = " " * indent
    if isinstance(value, dict):
        for key, item in value.items():
            _emit_entry(pad, key, item, indent, lines)
    elif isinstance(value, list):
        for item in value:
            if _is_scalar(item):
                lines.append("%s- %s" % (pad, yaml_scalar(item)))
            elif isinstance(item, dict) and all(_is_scalar(x) for x in item.values()):
                lines.append("%s- %s" % (pad, _flow_mapping(item)))
            elif isinstance(item, dict) and item:
                first = True
                for key, sub in item.items():
                    sub_lines = []
                    _emit_entry(" " * (indent + 2), key, sub, indent + 2, sub_lines)
                    if first:
                        # Put the first key on the dash line.
                        lines.append("%s- %s" % (pad, sub_lines[0].lstrip()))
                        first = False
                    else:
                        lines.extend(sub_lines)
            else:
                lines.append("%s-" % pad)
                _emit(item, indent + 2, lines)
    else:  # pragma: no cover - defensive
        lines.append("%s%s" % (pad, yaml_scalar(value)))


def _emit_entry(pad, key, item, indent, lines):
    if _is_scalar(item):
        lines.append("%s%s: %s" % (pad, yaml_scalar(key), yaml_scalar(item)))
    elif isinstance(item, list) and item and all(_is_scalar(x) for x in item):
        lines.append("%s%s: [%s]" % (pad, yaml_scalar(key), ", ".join(yaml_scalar(x) for x in item)))
    elif isinstance(item, dict) and all(_is_scalar(x) for x in item.values()):
        lines.append("%s%s: %s" % (pad, yaml_scalar(key), _flow_mapping(item)))
    else:
        lines.append("%s%s:" % (pad, yaml_scalar(key)))
        _emit(item, indent + 2, lines)


# --------------------------------------------------------------------------
# binutils readers
# --------------------------------------------------------------------------


class Binutils:
    def __init__(self, root):
        self.root = root
        self._cache = {}

    def text(self, relpath):
        if relpath not in self._cache:
            self._cache[relpath] = read_text(self.root, relpath)
        return self._cache[relpath]

    def stripped(self, relpath):
        return strip_comments(self.text(relpath))

    def version(self):
        """Best-effort binutils release number."""
        try:
            text = self.text("bfd/ChangeLog")
        except OSError:
            return "unknown"
        m = re.search(r"Bump version to ([0-9][0-9.]*)", text)
        if m:
            return m.group(1)
        m = re.search(r"^\s*\*\s*configure\.in:.*?([0-9]+\.[0-9]+)", text, re.M)
        if m:
            return m.group(1)
        return "unknown"


def elf_records(binutils, bits):
    """Container layouts of the ELF class, from include/elf/external.h."""
    suffix = "64" if bits == 64 else "32"
    header = binutils.text("include/elf/external.h")
    out = []

    def add(name, struct, desc, encodings=None):
        fields = parse_struct_fields_with_comments(header, struct)
        rec = {"name": name, "desc": desc, "size": sum(f[1] for f in fields), "fields": []}
        offset = 0
        for fname, fsize, comment in fields:
            entry = {"name": fname, "offset": offset, "size": fsize}
            if comment:
                entry["desc"] = comment
            rec["fields"].append(entry)
            offset += fsize
        if encodings:
            rec["encodings"] = encodings
        out.append(rec)

    add(
        "ehdr",
        "Elf%s_External_Ehdr" % suffix,
        "ELF%d file header" % bits,
    )
    add(
        "phdr",
        "Elf%s_External_Phdr" % suffix,
        "ELF%d program header" % bits,
    )
    add(
        "shdr",
        "Elf%s_External_Shdr" % suffix,
        "ELF%d section header" % bits,
    )
    add(
        "sym",
        "Elf%s_External_Sym" % suffix,
        "ELF%d symbol table entry" % bits,
        encodings=[
            # The value is shifted first and masked afterwards.
            {"field": "st_info", "target": "bind", "mask": 0x0F, "shift": 4},
            {"field": "st_info", "target": "type", "mask": 0x0F, "shift": 0},
            {"field": "st_other", "target": "visibility", "mask": 0x03, "shift": 0},
        ],
    )
    add(
        "rel",
        "Elf%s_External_Rel" % suffix,
        "ELF%d relocation entry (no addend)" % bits,
        encodings=_elf_rel_encodings(bits),
    )
    add(
        "rela",
        "Elf%s_External_Rela" % suffix,
        "ELF%d relocation entry (with addend)" % bits,
        encodings=_elf_rel_encodings(bits),
    )
    return out


def _elf_rel_encodings(bits):
    """How r_info is split into the relocation type and the symbol index.

    The value is shifted first and masked afterwards, so the mask describes
    the width of the extracted part.
    """
    if bits == 64:
        return [
            {"field": "r_info", "target": "type", "mask": 0xFFFFFFFF, "shift": 0},
            {"field": "r_info", "target": "symbol", "mask": 0xFFFFFFFF, "shift": 32},
        ]
    return [
        {"field": "r_info", "target": "type", "mask": 0xFF, "shift": 0},
        {"field": "r_info", "target": "symbol", "mask": 0xFFFFFF, "shift": 8},
    ]


def elf_constants(binutils):
    """The ELF constant groups shared by every ELF target."""
    common = binutils.text("include/elf/common.h")
    defines = parse_defines(common)
    groups = []

    def group(name, prefix, desc, skip=()):
        items = []
        for key in sorted(defines):
            if not key.startswith(prefix):
                continue
            if key in skip:
                continue
            value_text, comment = defines[key]
            try:
                value = int(value_text, 0)
            except ValueError:
                continue
            entry = {"name": key, "value": value}
            if comment:
                entry["desc"] = comment
            items.append(entry)
        if items:
            groups.append({"name": name, "desc": desc, "items": items})

    group("object_types", "ET_", "e_type values")
    group("machine_types", "EM_", "e_machine values")
    group("segment_types", "PT_", "p_type values")
    group("section_types", "SHT_", "sh_type values")
    group("section_flags", "SHF_", "sh_flags bits")
    group("symbol_bindings", "STB_", "st_info binding values")
    group("symbol_types", "STT_", "st_info type values")
    group("symbol_visibility", "STV_", "st_other visibility values")
    return groups


def elf_relocations(binutils, howto_file, table, numbers_file, prefix, label):
    """Relocation howto entries of one ELF emulation."""
    numbers = parse_reloc_numbers(binutils.text(numbers_file))
    return howto_relocations(binutils, howto_file, table, numbers, label)


def howto_relocations(binutils, howto_file, table, numbers, label, number_from="index"):
    """Convert a BFD reloc_howto_type table into ArchBFD relocation records.

    BFD looks a relocation up either by indexing the howto table with the
    relocation number (the ELF and ECOFF back ends) or through a type switch
    (the COFF back end).  `number_from` selects which one applies.  Entries
    without a name (EMPTY_HOWTO) are placeholders and are skipped; the
    numbering of the remaining entries is unaffected.
    """
    text = binutils.text(howto_file)
    entries = parse_howto_table(text, table)
    out = []
    for index, entry in enumerate(entries):
        if entry.get("empty"):
            continue
        type_name = entry["type"]
        number = index
        if number_from == "type":
            expected = numbers.get(type_name)
            if expected is not None:
                number = expected
        name = entry["name"]
        if not name:
            name = type_name if type_name and not type_name.isdigit() else "(empty:%d)" % index
        size_code = entry["size"]
        size = _SIZE_CODES.get(size_code, 0)
        rec = {
            "name": name,
            "number": number,
            "size": size,
            "bitsize": entry["bitsize"] or 0,
            "pcrel": bool(entry["pc_relative"]),
            "overflow": _OVERFLOW_NAMES.get(entry["overflow"], "dont"),
        }
        if entry["partial_inplace"]:
            rec["partial"] = True
        if entry["rightshift"]:
            rec["rightshift"] = entry["rightshift"]
        if entry["bitpos"]:
            rec["bitpos"] = entry["bitpos"]
        if entry["pcrel_offset"]:
            rec["pcrel_offset"] = True
        if entry["src_mask"]:
            rec["src_mask"] = entry["src_mask"]
        if entry["dst_mask"]:
            rec["dst_mask"] = entry["dst_mask"]
        if number_from == "index" and numbers:
            expected = numbers.get(type_name)
            if expected is not None and expected != number:
                rec["desc"] = "type %s is numbered %d in the header" % (type_name, expected)
        out.append(rec)
    return out


# --------------------------------------------------------------------------
# catalogue definition
# --------------------------------------------------------------------------

HEADER = """\
# {file} -- generated by tools/extract_bfd.py; do not edit by hand.
#
# Derived from the binutils {version} checkout ({root}):
{prov}
"""

WASM_HEADER = """\
# WAT.bfd.yaml -- generated by tools/extract_bfd.py; do not edit by hand.
#
# The WebAssembly binary format.  binutils {version} has no WebAssembly BFD
# back end, so this document does not come from the checkout; it follows the
# WebAssembly core specification:
{prov}
"""


def prov_lines(sources):
    return "\n".join("#   %s" % s for s in sources)


def build_specs(binutils):
    """Return the list of documents to generate."""
    specs = []

    # ---- ELF64 (x86-64 emulation) ------------------------------------
    elf64_relocs = elf_relocations(
        binutils, "bfd/elf64-x86-64.c", "x86_64_elf_howto_table", "include/elf/x86-64.h", "R_X86_64_", "x86-64"
    )
    specs.append(
        {
            "file": "ELF64.bfd.yaml",
            "doc": {
                "schema": 1,
                "name": "ELF64",
                "format": "elf",
                "description": "64-bit ELF objects: BFD elf64 target vector, x86-64 emulation.",
                "byte_order": "little",
                "target_vector": "elf64-x86-64",
                "arch": "x86-64",
                "magic": "7f454c46",
                "flags": ["host"],
                "source": [
                    {"file": "bfd/elfxx-target.h", "note": "elf64 target vector"},
                    {"file": "bfd/elf64.c", "note": "elf64 backend (ARCH_SIZE 64)"},
                    {"file": "bfd/elf64-x86-64.c", "note": "x86-64 relocation howto table"},
                    {"file": "include/elf/external.h", "note": "container layouts"},
                    {"file": "include/elf/common.h", "note": "ELF constants"},
                    {"file": "include/elf/x86-64.h", "note": "R_X86_64_* numbers"},
                ],
                "records": elf_records(binutils, 64),
                "constants": elf_constants(binutils),
                "relocations": elf64_relocs,
                "sections": [
                    {"name": ".text", "desc": "executable code"},
                    {"name": ".data", "desc": "initialised data"},
                    {"name": ".bss", "desc": "uninitialised data"},
                    {"name": ".rodata", "desc": "read-only data"},
                    {"name": ".symtab", "desc": "symbol table"},
                    {"name": ".strtab", "desc": "string table"},
                    {"name": ".rela.text", "desc": "relocations with addends for .text"},
                    {"name": ".plt", "desc": "procedure linkage table"},
                    {"name": ".got", "desc": "global offset table"},
                    {"name": ".dynamic", "desc": "dynamic linking information"},
                ],
            },
            "sources": [
                "bfd/elfxx-target.h",
                "bfd/elf64.c",
                "bfd/elf64-x86-64.c",
                "include/elf/external.h",
                "include/elf/common.h",
                "include/elf/x86-64.h",
            ],
        }
    )

    # ---- ELF32 (i386 emulation) --------------------------------------
    elf32_relocs = elf_relocations(
        binutils, "bfd/elf32-i386.c", "elf_howto_table", "include/elf/i386.h", "R_386_", "i386"
    )
    specs.append(
        {
            "file": "ELF32.bfd.yaml",
            "doc": {
                "schema": 1,
                "name": "ELF32",
                "format": "elf",
                "description": "32-bit ELF objects: BFD elf32 target vector, i386 emulation.",
                "byte_order": "little",
                "target_vector": "elf32-i386",
                "arch": "i386",
                "magic": "7f454c46",
                "flags": ["host"],
                "source": [
                    {"file": "bfd/elfxx-target.h", "note": "elf32 target vector"},
                    {"file": "bfd/elf32.c", "note": "elf32 backend (ARCH_SIZE 32)"},
                    {"file": "bfd/elf32-i386.c", "note": "i386 relocation howto table"},
                    {"file": "include/elf/external.h", "note": "container layouts"},
                    {"file": "include/elf/common.h", "note": "ELF constants"},
                    {"file": "include/elf/i386.h", "note": "R_386_* numbers"},
                ],
                "records": elf_records(binutils, 32),
                "constants": elf_constants(binutils),
                "relocations": elf32_relocs,
                "sections": [
                    {"name": ".text", "desc": "executable code"},
                    {"name": ".data", "desc": "initialised data"},
                    {"name": ".bss", "desc": "uninitialised data"},
                    {"name": ".rodata", "desc": "read-only data"},
                    {"name": ".symtab", "desc": "symbol table"},
                    {"name": ".strtab", "desc": "string table"},
                    {"name": ".rel.text", "desc": "relocations without addends for .text"},
                    {"name": ".plt", "desc": "procedure linkage table"},
                    {"name": ".got", "desc": "global offset table"},
                    {"name": ".dynamic", "desc": "dynamic linking information"},
                ],
            },
            "sources": [
                "bfd/elfxx-target.h",
                "bfd/elf32.c",
                "bfd/elf32-i386.c",
                "include/elf/external.h",
                "include/elf/common.h",
                "include/elf/i386.h",
            ],
        }
    )

    # ---- ELF32 HPPA emulation ----------------------------------------
    hppa_relocs = elf_relocations(
        binutils, "bfd/elf-hppa.h", "elf_hppa_howto_table", "include/elf/hppa.h", "R_PARISC_", "hppa"
    )
    specs.append(
        {
            "file": "hppa.bfd.yaml",
            "doc": {
                "schema": 1,
                "name": "hppa",
                "format": "elf",
                "container": "ELF32",
                "description": "ELF32 objects for the HP PA-RISC emulation (bfd/elf32-hppa.c).",
                "byte_order": "big",
                "target_vector": "elf32-hppa",
                "arch": "hppa",
                "magic": "7f454c46",
                "flags": ["host"],
                "source": [
                    {"file": "bfd/elf32-hppa.c", "note": "elf32-hppa target vector"},
                    {"file": "bfd/elf-hppa.h", "note": "R_PARISC_* relocation howto table"},
                    {"file": "include/elf/hppa.h", "note": "R_PARISC_* numbers"},
                ],
                "relocations": hppa_relocs,
                "sections": [
                    {"name": ".text", "desc": "executable code"},
                    {"name": ".data", "desc": "initialised data"},
                    {"name": ".bss", "desc": "uninitialised data"},
                    {"name": ".rodata", "desc": "read-only data"},
                    {"name": ".symtab", "desc": "symbol table"},
                    {"name": ".strtab", "desc": "string table"},
                    {"name": ".rel.text", "desc": "relocations without addends for .text"},
                ],
            },
            "sources": [
                "bfd/elf32-hppa.c",
                "bfd/elf-hppa.h",
                "include/elf/hppa.h",
            ],
        }
    )

    # ---- ELF32 m68k emulation -----------------------------------------
    m68k_relocs = elf_relocations(
        binutils, "bfd/elf32-m68k.c", "howto_table", "include/elf/m68k.h", "R_68K_", "m68k"
    )
    specs.append(
        {
            "file": "m68k.bfd.yaml",
            "doc": {
                "schema": 1,
                "name": "m68k",
                "format": "elf",
                "container": "ELF32",
                "description": "ELF32 objects for the Motorola 68000 emulation (bfd/elf32-m68k.c).",
                "byte_order": "big",
                "target_vector": "elf32-m68k",
                "arch": "m68k",
                "magic": "7f454c46",
                "flags": ["host"],
                "source": [
                    {"file": "bfd/elf32-m68k.c", "note": "elf32-m68k target vector and howto table"},
                    {"file": "include/elf/m68k.h", "note": "R_68K_* numbers"},
                ],
                "relocations": m68k_relocs,
                "sections": [
                    {"name": ".text", "desc": "executable code"},
                    {"name": ".data", "desc": "initialised data"},
                    {"name": ".bss", "desc": "uninitialised data"},
                    {"name": ".rodata", "desc": "read-only data"},
                    {"name": ".symtab", "desc": "symbol table"},
                    {"name": ".strtab", "desc": "string table"},
                    {"name": ".rel.text", "desc": "relocations without addends for .text"},
                ],
            },
            "sources": [
                "bfd/elf32-m68k.c",
                "include/elf/m68k.h",
            ],
        }
    )

    # ---- COFF --------------------------------------------------------
    coff_records = coff_container_records(binutils)
    coff_relocs = coff_howto(binutils)
    specs.append(
        {
            "file": "COFF.bfd.yaml",
            "doc": {
                "schema": 1,
                "name": "COFF",
                "format": "coff",
                "description": "Common Object File Format: BFD coff target vector, i386 emulation.",
                "byte_order": "little",
                "target_vector": "coff-i386",
                "arch": "i386",
                # I386MAGIC (0x014c) in file order.
                "magic": "4c01",
                "source": [
                    {"file": "include/coff/external.h", "note": "COFF container layouts"},
                    {"file": "include/coff/i386.h", "note": "COFF relocation entry and magic numbers"},
                    {"file": "include/coff/internal.h", "note": "COFF relocation type numbers"},
                    {"file": "bfd/coff-i386.c", "note": "i386 COFF relocation howto table"},
                ],
                "records": coff_records,
                "constants": coff_constants(binutils),
                "relocations": coff_relocs,
                "sections": [
                    {"name": ".text", "desc": "executable code"},
                    {"name": ".data", "desc": "initialised data"},
                    {"name": ".bss", "desc": "uninitialised data"},
                    {"name": ".comment", "desc": "compiler version"},
                ],
            },
            "sources": [
                "include/coff/external.h",
                "include/coff/i386.h",
                "include/coff/internal.h",
                "bfd/coff-i386.c",
            ],
        }
    )

    # ---- PE ----------------------------------------------------------
    pe_records = pe_container_records(binutils)
    specs.append(
        {
            "file": "PE.bfd.yaml",
            "doc": {
                "schema": 1,
                "name": "PE",
                "format": "pe",
                "container": "COFF",
                "description": "Portable Executable: PE32/PE32+ images (BFD pei target vectors).",
                "byte_order": "little",
                "target_vector": "pei-i386",
                "arch": "i386",
                "magic": "4d5a",
                "source": [
                    {"file": "include/coff/pe.h", "note": "PE/COFF file header layouts"},
                    {"file": "bfd/peicode.h", "note": "PE optional header and relocations"},
                ],
                "records": pe_records,
                "constants": pe_constants(binutils),
                "sections": [
                    {"name": ".text", "desc": "executable code"},
                    {"name": ".data", "desc": "initialised data"},
                    {"name": ".bss", "desc": "uninitialised data"},
                    {"name": ".rdata", "desc": "read-only data"},
                    {"name": ".idata", "desc": "import data"},
                    {"name": ".rsrc", "desc": "resources"},
                ],
            },
            "sources": [
                "include/coff/pe.h",
                "bfd/peicode.h",
            ],
        }
    )

    # ---- Mach-O -------------------------------------------------------
    macho_records = macho_container_records(binutils)
    specs.append(
        {
            "file": "MachO.bfd.yaml",
            "doc": {
                "schema": 1,
                "name": "MachO",
                "format": "macho",
                "description": "Mach-O objects and images (BFD mach-o target vector).",
                "byte_order": "little",
                "target_vector": "mach-o",
                "arch": "i386",
                "magic": "cefaedfe",
                "source": [
                    {"file": "include/mach-o/external.h", "note": "Mach-O container layouts"},
                    {"file": "include/mach-o/loader.h", "note": "Mach-O constants"},
                    {"file": "bfd/mach-o.c", "note": "Mach-O backend"},
                ],
                "records": macho_records,
                "constants": macho_constants(binutils),
                "sections": [
                    {"name": "__TEXT", "desc": "text segment"},
                    {"name": "__DATA", "desc": "data segment"},
                    {"name": "__bss", "desc": "uninitialised data"},
                    {"name": "__DWARF", "desc": "debug information"},
                ],
            },
            "sources": [
                "include/mach-o/external.h",
                "include/mach-o/loader.h",
                "bfd/mach-o.c",
            ],
        }
    )

    # ---- ECOFF / IRIX --------------------------------------------------
    ecoff_records = ecoff_container_records(binutils)
    mips_relocs = ecoff_relocations(binutils)
    specs.append(
        {
            "file": "irix.bfd.yaml",
            "doc": {
                "schema": 1,
                "name": "irix",
                "format": "ecoff",
                "description": "IRIX ECOFF objects: BFD ecoff-bigmips target vector.",
                "byte_order": "big",
                "target_vector": "ecoff-bigmips",
                "arch": "mips",
                "magic": "0160",
                "flags": ["host"],
                "source": [
                    {"file": "bfd/coff-mips.c", "note": "MIPS ECOFF target vector and howto table"},
                    {"file": "bfd/ecoff.c", "note": "ECOFF backend"},
                    {"file": "include/coff/mips.h", "note": "ECOFF container layouts and MIPS_R_* types"},
                    {"file": "include/coff/external.h", "note": "ECOFF file and section headers"},
                ],
                "records": ecoff_records,
                "constants": ecoff_constants(binutils),
                "relocations": mips_relocs,
                "sections": [
                    {"name": ".text", "desc": "executable code"},
                    {"name": ".data", "desc": "initialised data"},
                    {"name": ".bss", "desc": "uninitialised data"},
                    {"name": ".mdebug", "desc": "MIPS ECOFF debug information"},
                ],
            },
            "sources": [
                "bfd/coff-mips.c",
                "bfd/ecoff.c",
                "include/coff/mips.h",
                "include/coff/external.h",
            ],
        }
    )

    # ---- WebAssembly ----------------------------------------------------
    # binutils 2.22 has no WebAssembly BFD back end, so this document is not
    # derived from the checkout: it follows the WebAssembly core
    # specification (binary format) and is listed here so that the catalogue
    # covers the object format the MetaArch "wasm" architecture uses.
    specs.append(
        {
            "file": "WAT.bfd.yaml",
            "header": WASM_HEADER,
            "doc": {
                "schema": 1,
                "name": "WAT",
                "format": "wasm",
                "description": "WebAssembly binary format (wasm modules).",
                "byte_order": "little",
                "target_vector": "wasm32",
                "arch": "wasm",
                "magic": "0061736d",
                "source": [
                    {"file": "wasm/core/binary.rst",
                     "note": "WebAssembly core specification, binary format"},
                ],
                "records": [
                    {
                        "name": "header",
                        "desc": "WebAssembly module header",
                        "size": 8,
                        "fields": [
                            {"name": "magic", "offset": 0, "size": 4,
                             "desc": "\\0asm"},
                            {"name": "version", "offset": 4, "size": 4,
                             "desc": "binary format version, 1"},
                        ],
                    },
                    {
                        "name": "section",
                        "desc": "WebAssembly section header",
                        "size": 1,
                        "fields": [
                            {"name": "id", "offset": 0, "size": 1,
                             "desc": "section id"},
                        ],
                    },
                ],
                "constants": [
                    {
                        "name": "section_types",
                        "desc": "WebAssembly section ids",
                        "items": [
                            {"name": "custom", "value": 0, "desc": "custom section"},
                            {"name": "type", "value": 1, "desc": "type section"},
                            {"name": "import", "value": 2, "desc": "import section"},
                            {"name": "function", "value": 3, "desc": "function section"},
                            {"name": "table", "value": 4, "desc": "table section"},
                            {"name": "memory", "value": 5, "desc": "memory section"},
                            {"name": "global", "value": 6, "desc": "global section"},
                            {"name": "export", "value": 7, "desc": "export section"},
                            {"name": "start", "value": 8, "desc": "start section"},
                            {"name": "element", "value": 9, "desc": "element section"},
                            {"name": "code", "value": 10, "desc": "code section"},
                            {"name": "data", "value": 11, "desc": "data section"},
                            {"name": "data_count", "value": 12,
                             "desc": "data count section"},
                        ],
                    }
                ],
                "sections": [
                    {"name": "custom", "desc": "custom section (name, reloc.*)"},
                    {"name": "code", "desc": "function bodies"},
                    {"name": "data", "desc": "data segments"},
                ],
            },
            "sources": [
                "WebAssembly core specification, binary format",
            ],
        }
    )

    return specs


# --------------------------------------------------------------------------
# per-format container helpers
# --------------------------------------------------------------------------


def _record(name, desc, fields, encodings=None, limit=None):
    if limit is not None:
        fields = fields[:limit]
    rec = {"name": name, "desc": desc, "size": sum(f["size"] for f in fields), "fields": fields}
    if encodings:
        rec["encodings"] = encodings
    return rec


def _fields_from(header_text, struct, offset=0):
    fields = []
    defines = _numeric_defines(header_text)
    for fname, fsize, comment in parse_struct_fields_with_comments(header_text, struct, defines):
        entry = {"name": fname, "offset": offset, "size": fsize}
        if comment:
            entry["desc"] = comment
        fields.append(entry)
        offset += fsize
    return fields


def _numeric_defines(text):
    """{macro: value} for the simple integer #defines of a header."""
    out = {}
    for name, (value_text, _comment) in parse_defines(text).items():
        try:
            out[name] = int(value_text, 0)
        except ValueError:
            continue
    return out


def coff_container_records(binutils):
    external = binutils.text("include/coff/external.h")
    i386 = binutils.text("include/coff/i386.h")
    records = []
    records.append(
        _record(
            "filehdr",
            "COFF file header",
            _fields_from(external, "external_filehdr"),
        )
    )
    records.append(
        _record(
            "scnhdr",
            "COFF section header",
            _fields_from(external, "external_scnhdr"),
        )
    )
    records.append(
        _record(
            "syment",
            "COFF symbol table entry",
            _fields_from(external, "external_syment"),
            encodings=[
                {"field": "e_type", "target": "type", "mask": 0xFFFF, "shift": 0},
                {"field": "e_sclass", "target": "class", "mask": 0xFF, "shift": 0},
            ],
        )
    )
    records.append(
        _record(
            "reloc",
            "COFF relocation entry",
            _fields_from(i386, "external_reloc"),
        )
    )
    return records


def coff_constants(binutils):
    """The COFF relocation type numbers (include/coff/internal.h)."""
    defines = parse_defines(binutils.text("include/coff/internal.h"))
    items = []
    for key in sorted(defines):
        if not key.startswith("R_"):
            continue
        value_text, comment = defines[key]
        try:
            value = int(value_text, 0)
        except ValueError:
            continue
        entry = {"name": key, "value": value}
        if comment:
            entry["desc"] = comment
        items.append(entry)
    magic = []
    i386 = parse_defines(binutils.text("include/coff/i386.h"))
    for key in ("I386MAGIC", "I386PTXMAGIC", "I386AIXMAGIC", "LYNXCOFFMAGIC"):
        if key in i386:
            try:
                magic.append({"name": key, "value": int(i386[key][0], 0)})
            except ValueError:
                pass
    classes = []
    for key, (value_text, comment) in defines.items():
        if not key.startswith("C_"):
            continue
        try:
            classes.append({"name": key, "value": int(value_text, 0)})
        except ValueError:
            pass
    groups = [{"name": "reloc_types", "desc": "COFF relocation types", "items": items}]
    if classes:
        groups.append({"name": "storage_classes", "desc": "COFF symbol storage classes",
                       "items": classes})
    if magic:
        groups.append({"name": "magic_numbers", "desc": "COFF file magic numbers",
                       "items": magic})
    return groups


def coff_howto(binutils):
    """The i386 COFF relocation howto table (bfd/coff-i386.c)."""
    defines = parse_defines(binutils.text("include/coff/internal.h"))
    numbers = {}
    for key, (value_text, _comment) in defines.items():
        try:
            numbers[key] = int(value_text, 0)
        except ValueError:
            pass
    return howto_relocations(binutils, "bfd/coff-i386.c", "howto_table", numbers, "i386",
                             number_from="type")


def pe_optional_header(bits):
    """PE32 / PE32+ optional header layout.

    BFD keeps the optional header in `struct internal_extra_pe_aouthdr`
    (include/coff/internal.h) and writes it in bfd/peXXigen.c; the byte
    layout below is the PE/COFF one those two files implement.  The BFD
    "a.out" slots (magic, vstamp, tsize, dsize, bsize, entry, text_start,
    data_start) carry the PE fields listed in the comments.
    """
    fields = [
        ("Magic", 2, "PE signature: 0x10b (PE32) or 0x20b (PE32+)"),
        ("MajorLinkerVersion", 1, "linker major version"),
        ("MinorLinkerVersion", 1, "linker minor version"),
        ("SizeOfCode", 4, "sum of the code section sizes"),
        ("SizeOfInitializedData", 4, "sum of the initialised data sizes"),
        ("SizeOfUninitializedData", 4, "sum of the uninitialised data sizes"),
        ("AddressOfEntryPoint", 4, "entry point relative to the image base"),
        ("BaseOfCode", 4, "address of the first code section"),
    ]
    if bits == 32:
        fields.append(("BaseOfData", 4, "address of the first data section"))
        fields.append(("ImageBase", 4, "preferred image base"))
    else:
        fields.append(("ImageBase", 8, "preferred image base"))
    fields += [
        ("SectionAlignment", 4, "alignment of sections when loaded"),
        ("FileAlignment", 4, "alignment of raw data in the file"),
        ("MajorOperatingSystemVersion", 2, "required major OS version"),
        ("MinorOperatingSystemVersion", 2, "required minor OS version"),
        ("MajorImageVersion", 2, "major image version"),
        ("MinorImageVersion", 2, "minor image version"),
        ("MajorSubsystemVersion", 2, "required major subsystem version"),
        ("MinorSubsystemVersion", 2, "required minor subsystem version"),
        ("Win32VersionValue", 4, "reserved, must be zero"),
        ("SizeOfImage", 4, "size of the image in memory"),
        ("SizeOfHeaders", 4, "size of the headers rounded to FileAlignment"),
        ("CheckSum", 4, "image checksum"),
        ("Subsystem", 2, "subsystem the image runs in"),
        ("DllCharacteristics", 2, "DLL characteristics flags"),
    ]
    wide = 8 if bits == 64 else 4
    fields += [
        ("SizeOfStackReserve", wide, "stack space to reserve"),
        ("SizeOfStackCommit", wide, "stack space to commit"),
        ("SizeOfHeapReserve", wide, "heap space to reserve"),
        ("SizeOfHeapCommit", wide, "heap space to commit"),
        ("LoaderFlags", 4, "loader flags, must be zero"),
        ("NumberOfRvaAndSizes", 4, "number of data directory entries"),
    ]
    out = []
    offset = 0
    for name, size, desc in fields:
        out.append({"name": name, "offset": offset, "size": size, "desc": desc})
        offset += size
    return out


def pe_container_records(binutils):
    header = binutils.text("include/coff/pe.h")
    records = []
    records.append(
        _record(
            "filehdr",
            "PE/COFF file header (DOS stub plus COFF header)",
            _fields_from(header, "external_PEI_filehdr"),
        )
    )
    records.append(
        _record(
            "optional_header",
            "PE32 optional header",
            pe_optional_header(32),
        )
    )
    records.append(
        _record(
            "optional_header_64",
            "PE32+ optional header",
            pe_optional_header(64),
        )
    )
    records.append(
        _record(
            "data_directory",
            "PE data directory entry",
            [
                {"name": "VirtualAddress", "offset": 0, "size": 4,
                 "desc": "relative virtual address of the table"},
                {"name": "Size", "offset": 4, "size": 4, "desc": "size of the table"},
            ],
        )
    )
    return records


def pe_constants(binutils):
    header = binutils.text("include/coff/pe.h")
    defines = parse_defines(header)
    groups = []

    def group(name, prefix, desc):
        items = []
        for key in sorted(defines):
            if not key.startswith(prefix):
                continue
            value_text, comment = defines[key]
            try:
                value = int(value_text, 0)
            except ValueError:
                continue
            entry = {"name": key, "value": value}
            if comment:
                entry["desc"] = comment
            items.append(entry)
        if items:
            groups.append({"name": name, "desc": desc, "items": items})

    group("machine_types", "IMAGE_FILE_MACHINE_", "FILE_HEADER.Machine values")
    group("characteristics", "IMAGE_FILE_", "FILE_HEADER.Characteristics bits")
    return groups


def macho_container_records(binutils):
    header = binutils.text("include/mach-o/external.h")
    records = []
    records.append(
        _record(
            "header",
            "Mach-O header (32-bit)",
            _fields_from(header, "mach_o_header_external"),
            limit=7,
        )
    )
    records.append(
        _record(
            "header_64",
            "Mach-O header (64-bit, with the reserved field)",
            _fields_from(header, "mach_o_header_external"),
        )
    )
    records.append(
        _record(
            "load_command",
            "Mach-O load command",
            _fields_from(header, "mach_o_load_command_external"),
        )
    )
    records.append(
        _record(
            "segment_command",
            "Mach-O 32-bit segment command",
            _fields_from(header, "mach_o_segment_command_32_external"),
        )
    )
    records.append(
        _record(
            "segment_command_64",
            "Mach-O 64-bit segment command",
            _fields_from(header, "mach_o_segment_command_64_external"),
        )
    )
    records.append(
        _record(
            "section",
            "Mach-O 32-bit section header",
            _fields_from(header, "mach_o_section_32_external"),
        )
    )
    records.append(
        _record(
            "section_64",
            "Mach-O 64-bit section header",
            _fields_from(header, "mach_o_section_64_external"),
        )
    )
    records.append(
        _record(
            "symtab_command",
            "Mach-O symbol table command",
            _fields_from(header, "mach_o_symtab_command_external"),
        )
    )
    records.append(
        _record(
            "nlist",
            "Mach-O 32-bit symbol table entry",
            _fields_from(header, "mach_o_nlist_external"),
        )
    )
    records.append(
        _record(
            "nlist_64",
            "Mach-O 64-bit symbol table entry",
            _fields_from(header, "mach_o_nlist_64_external"),
        )
    )
    records.append(
        _record(
            "reloc",
            "Mach-O relocation entry",
            _fields_from(header, "mach_o_reloc_info_external"),
        )
    )
    return records


def macho_constants(binutils):
    loader = binutils.text("include/mach-o/loader.h")
    reloc = binutils.text("include/mach-o/reloc.h")
    defines = parse_defines(loader)
    reloc_defines = parse_defines(reloc)
    enums = parse_enums(loader)
    groups = []

    def group(name, source, prefix, desc):
        items = []
        for key in sorted(source):
            if not key.startswith(prefix):
                continue
            value_text = source[key][0]
            try:
                value = int(value_text, 0)
            except ValueError:
                continue
            items.append({"name": key, "value": value})
        if items:
            groups.append({"name": name, "desc": desc, "items": items})

    def enum_group(name, prefix, desc):
        items = []
        for key in sorted(enums):
            if not key.startswith(prefix):
                continue
            items.append({"name": key, "value": enums[key]})
        if items:
            groups.append({"name": name, "desc": desc, "items": items})

    enum_group("load_commands", "BFD_MACH_O_LC_", "load command types")
    enum_group("file_types", "BFD_MACH_O_MH_", "file type values")
    enum_group("cpu_types", "BFD_MACH_O_CPU_TYPE_", "CPU type values")
    enum_group("header_flags", "BFD_MACH_O_MH_", "header flag values")
    group("reloc_types", reloc_defines, "BFD_MACH_O_GENERIC_RELOC_",
          "generic Mach-O relocation types")
    group("reloc_flags", reloc_defines, "BFD_MACH_O_",
          "Mach-O relocation bit fields")
    return groups


def ecoff_container_records(binutils):
    """ECOFF (IRIX) container layouts, from include/coff/mips.h."""
    external = binutils.text("include/coff/external.h")
    mips = binutils.text("include/coff/mips.h")
    records = []
    records.append(
        _record(
            "filehdr",
            "ECOFF file header",
            _fields_from(external, "external_filehdr"),
        )
    )
    records.append(
        _record(
            "aouthdr",
            "ECOFF optional header",
            _fields_from(mips, "external_aouthdr"),
        )
    )
    records.append(
        _record(
            "reloc",
            "ECOFF relocation entry (8 bytes: address plus packed bits)",
            _fields_from(mips, "external_reloc"),
        )
    )
    return records


def ecoff_constants(binutils):
    """ECOFF magic numbers and MIPS relocation types."""
    ecoff = parse_defines(binutils.text("include/coff/ecoff.h"))
    mips = parse_defines(binutils.text("include/coff/mips.h"))
    groups = []

    def group(name, source, prefix, desc):
        items = []
        for key in sorted(source):
            if not key.startswith(prefix):
                continue
            value_text, comment = source[key]
            try:
                value = int(value_text, 0)
            except ValueError:
                continue
            entry = {"name": key, "value": value}
            if comment:
                entry["desc"] = comment
            items.append(entry)
        if items:
            groups.append({"name": name, "desc": desc, "items": items})

    group("magic_numbers", ecoff, "MIPS_MAGIC_", "ECOFF file magic numbers")
    group("reloc_types", mips, "MIPS_R_", "MIPS ECOFF relocation types")
    return groups


def ecoff_relocations(binutils):
    """The MIPS ECOFF relocation howto table (bfd/coff-mips.c)."""
    defines = parse_defines(binutils.text("include/coff/mips.h"))
    numbers = {}
    for key, (value_text, _comment) in defines.items():
        try:
            numbers[key] = int(value_text, 0)
        except ValueError:
            pass
    return howto_relocations(binutils, "bfd/coff-mips.c", "mips_howto_table", numbers, "mips")


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------


def render(spec, version, root_label):
    header = (spec.get("header") or HEADER).format(
        file=spec["file"],
        version=version,
        root=root_label,
        prov=prov_lines(spec["sources"]),
    )
    return header + "\n" + emit_yaml(spec["doc"])


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    parser.add_argument("--binutils", default=os.path.join(here, "binutils"))
    parser.add_argument("--out", default=os.path.join(here, "ArchBFD"))
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)

    if not os.path.isdir(args.binutils):
        sys.stderr.write("extract_bfd: no binutils checkout at %s\n" % args.binutils)
        return 2

    binutils = Binutils(args.binutils)
    version = binutils.version()
    specs = build_specs(binutils)

    status = 0
    for spec in specs:
        text = render(spec, version, os.path.relpath(args.binutils, os.path.dirname(args.out) or "."))
        path = os.path.join(args.out, spec["file"])
        if args.check:
            try:
                with open(path, "r", encoding="utf-8") as fh:
                    current = fh.read()
            except OSError as exc:
                sys.stderr.write("extract_bfd: %s\n" % exc)
                status = 1
                continue
            if current != text:
                sys.stderr.write("extract_bfd: %s is out of date\n" % path)
                status = 1
        else:
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(text)
            sys.stdout.write("wrote %s\n" % path)
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
