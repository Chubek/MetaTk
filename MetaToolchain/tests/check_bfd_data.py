#!/usr/bin/env python3
"""Cross-check the ArchBFD data against real object files.

The descriptions in ArchBFD/*.bfd.yaml are extracted from binutils/bfd, but
the *meaning* of the numbers (record sizes, relocation numbers, the way the
packed fields are split) has to agree with the objects a toolchain produces.
This script compiles a small C file with the system compiler and checks the
resulting ELF64 object against the ELF64 description.

The check needs a C compiler; without one it exits with 77 (skipped).

    python3 tests/check_bfd_data.py [ARCHBFD_DIR]
"""

import os
import struct
import subprocess
import sys
import tempfile

try:
    import yaml
except ImportError:  # pragma: no cover - depends on the environment
    sys.stderr.write("check_bfd_data: PyYAML is not installed\n")
    sys.exit(77)

SOURCE = """
int global;
int add(int a, int b) { return a + b + global; }
"""


def fail(message):
    sys.stderr.write("check_bfd_data: %s\n" % message)
    return 1


def load_document(root, name):
    path = os.path.join(root, "%s.bfd.yaml" % name)
    with open(path, "r", encoding="utf-8") as fh:
        return yaml.safe_load(fh)


def record(document, name):
    for rec in document.get("records", []):
        if rec.get("name") == name:
            return rec
    return None


def constant(document, group, name):
    for grp in document.get("constants", []):
        if grp.get("name") != group:
            continue
        for item in grp.get("items", []):
            if item.get("name") == name:
                return item.get("value")
    return None


def relocation(document, name):
    for rel in document.get("relocations", []):
        if rel.get("name") == name:
            return rel
    return None


def main(argv):
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    root = argv[0] if argv else os.path.join(here, "ArchBFD")
    compiler = os.environ.get("CC", "cc")
    try:
        subprocess.run([compiler, "--version"], check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
    except (OSError, subprocess.CalledProcessError):
        sys.stderr.write("check_bfd_data: no C compiler (%s); skipped\n" % compiler)
        return 77

    document = load_document(root, "ELF64")
    if document is None:
        return fail("ELF64.bfd.yaml not found in %s" % root)

    errors = 0
    with tempfile.TemporaryDirectory() as tmp:
        source = os.path.join(tmp, "check.c")
        obj = os.path.join(tmp, "check.o")
        with open(source, "w", encoding="utf-8") as fh:
            fh.write(SOURCE)
        try:
            subprocess.run([compiler, "-c", "-O0", source, "-o", obj], check=True)
        except (OSError, subprocess.CalledProcessError) as exc:
            sys.stderr.write("check_bfd_data: compilation failed: %s\n" % exc)
            return 77
        with open(obj, "rb") as fh:
            data = fh.read()

    # --- the file header ----------------------------------------------------
    ehdr = record(document, "ehdr")
    if ehdr is None:
        return fail("the ELF64 document has no ehdr record")
    if ehdr["size"] != 64:
        errors += fail("the ELF64 file header is described as %d bytes" % ehdr["size"])
    if data[:4] != b"\x7fELF":
        errors += fail("the object does not start with the ELF magic")
    if data[4] != 2:
        errors += fail("the object is not ELFCLASS64")
    if data[5] != 1:
        errors += fail("the object is not little endian")

    (e_type, e_machine, e_version, e_entry, e_phoff, e_shoff, e_flags, e_ehsize,
     e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx) = struct.unpack_from(
        "<HHIQQQIHHHHHH", data, 16)
    if e_ehsize != ehdr["size"]:
        errors += fail("e_ehsize is %d but the record says %d" % (e_ehsize, ehdr["size"]))
    if e_shentsize != 64:
        errors += fail("e_shentsize is %d" % e_shentsize)
    shdr = record(document, "shdr")
    if shdr is None or shdr["size"] != e_shentsize:
        errors += fail("the shdr record does not match e_shentsize (%d)" % e_shentsize)
    if e_machine != constant(document, "machine_types", "EM_X86_64"):
        errors += fail("e_machine is %d but EM_X86_64 is %s" %
                       (e_machine, constant(document, "machine_types", "EM_X86_64")))

    # --- the section headers ------------------------------------------------
    sections = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        (sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info,
         sh_addralign, sh_entsize) = struct.unpack_from("<IIQQQQIIQQ", data, off)
        sections.append({
            "name_off": sh_name, "type": sh_type, "offset": sh_offset, "size": sh_size,
            "link": sh_link, "info": sh_info, "entsize": sh_entsize,
        })
    strtab = sections[e_shstrndx]
    names = data[strtab["offset"]:strtab["offset"] + strtab["size"]]

    def section_name(index):
        entry = sections[index]
        end = names.find(b"\0", entry["name_off"])
        return names[entry["name_off"]:end].decode("ascii", "replace")

    # --- the symbol table ---------------------------------------------------
    symtabs = []
    relas = []
    for i, sec in enumerate(sections):
        if sec["type"] == constant(document, "section_types", "SHT_SYMTAB"):
            symtabs.append((i, sec))
        if sec["type"] == constant(document, "section_types", "SHT_RELA"):
            relas.append((i, sec))
    if not symtabs:
        errors += fail("the object has no symbol table")
    if not relas:
        errors += fail("the object has no .rela section")
    if symtabs and relas:
        symrec = record(document, "sym")
        relarec = record(document, "rela")
        if symrec is None or symrec["size"] != 24:
            errors += fail("the sym record is not 24 bytes")
        if relarec is None or relarec["size"] != 24:
            errors += fail("the rela record is not 24 bytes")

        index, sec = symtabs[0]
        if sec["entsize"] != 24:
            errors += fail("the symbol table entry size is %d" % sec["entsize"])
        strsec = sections[sec["link"]]
        strdata = data[strsec["offset"]:strsec["offset"] + strsec["size"]]
        symdata = data[sec["offset"]:sec["offset"] + sec["size"]]

        # The first symbol must be the null symbol.
        st_name, st_info, st_other, st_shndx, st_value, st_size = struct.unpack_from(
            "<IBBHQQ", symdata, 0)
        if st_name != 0 or st_value != 0:
            errors += fail("the first symbol is not the null symbol")

        # Every relocation type used by the object must exist in the
        # description with the same number, and the symbol index must sit in
        # the upper half of r_info.
        by_number = {}
        for rel in document.get("relocations", []):
            by_number.setdefault(rel.get("number"), rel)
        seen_global = False
        for index, sec in relas:
            if sec["entsize"] != 24:
                errors += fail("the relocation entry size is %d" % sec["entsize"])
            for i in range(sec["size"] // sec["entsize"]):
                r_offset, r_info, r_addend = struct.unpack_from(
                    "<QQq", data, sec["offset"] + i * sec["entsize"])
                rtype = r_info & 0xFFFFFFFF
                rsym = r_info >> 32
                if rtype not in by_number:
                    errors += fail("relocation type %d is not in the description" % rtype)
                if rsym * 24 >= len(symdata):
                    errors += fail("relocation %d points outside the symbol table" % i)
                    continue
                st_name, st_info, st_other, st_shndx, st_value, st_size = struct.unpack_from(
                    "<IBBHQQ", symdata, rsym * 24)
                end = strdata.find(b"\0", st_name)
                name = strdata[st_name:end].decode("ascii", "replace")
                if name == "global":
                    seen_global = True
                    rel = by_number.get(rtype)
                    if rel is None or rel.get("name") != "R_X86_64_PC32":
                        errors += fail("the relocation against 'global' is not R_X86_64_PC32")
                    elif rel["size"] != 4:
                        errors += fail("R_X86_64_PC32 is described as %d bytes" % rel["size"])
                    elif not rel.get("pcrel"):
                        errors += fail("R_X86_64_PC32 is not marked pc-relative")
        if not seen_global:
            errors += fail("the object has no relocation against 'global'")

    if errors:
        print("%d problem(s) found" % errors)
        return 1
    print("ELF64 description agrees with the object file produced by %s" % compiler)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
