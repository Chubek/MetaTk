import archbfd;
import std.file : read;
import std.stdio : writeln;
import std.string : fromStringz, toStringz;

int main(string[] args) {
    assert(args.length == 4); // the program name plus three arguments
    DsymDiags d;
    dsym_diags_init(&d);
    scope(exit) dsym_diags_free(&d);

    // The catalogue owns the documents it loads.
    auto db = archbfd_open(toStringz(args[2]), &d);
    assert(db !is null);
    scope(exit) archbfd_close(db);
    assert(archbfd_count(db) >= 8);

    auto spec = archbfd_load_name(db, "ELF64", 0, &d);
    assert(spec !is null);
    assert(archbfd_find_reloc(spec, "R_X86_64_PC32") >= 0);
    assert(archbfd_find_reloc_number(spec, 2) >= 0);
    assert(archbfd_find_reloc(spec, "R_X86_64_NOPE") == -1);
    assert(archbfd_find_record(spec, "ehdr") !is null);
    assert(archbfd_resolve_record(spec, "ehdr") !is null);

    // The enumeration spellings.
    assert(fromStringz(archbfd_format_name(ARCHBFD_FMT_ELF)) == "elf");
    assert(fromStringz(archbfd_format_name(ARCHBFD_FMT_PE)) == "pe");
    assert(fromStringz(archbfd_byte_order_name(ARCHBFD_BO_LITTLE)) == "little");
    assert(fromStringz(archbfd_byte_order_name(ARCHBFD_BO_BIG)) == "big");
    assert(fromStringz(archbfd_overflow_name(ARCHBFD_OVF_SIGNED)) == "signed");

    // A document loaded on its own is owned by the caller.
    auto own = archbfd_spec_load_file(toStringz(args[1] ~ "/ELF64.bfd.yaml"), 0, 0, &d);
    assert(own !is null);
    scope(exit) archbfd_spec_free(own);
    assert(archbfd_find_reloc(own, "R_X86_64_PC32") >= 0);

    // The object reader, when the test object is available.
    auto bytes = read(args[3]);
    if (bytes.length == 0) {
        writeln("D ArchBFD interface tests passed (no object file)");
        return 0;
    }
    auto object = archbfd_object_open_memory(bytes.ptr, bytes.length, spec, &d);
    assert(object !is null);
    scope(exit) archbfd_object_close(object);
    assert(fromStringz(archbfd_object_kind(object)) == "elf64");
    assert(archbfd_object_machine(object) == 62);
    assert(archbfd_object_type(object) == 1);
    assert(archbfd_object_section_count(object) == 6);
    assert(archbfd_object_symbol_count(object) == 3);
    assert(archbfd_object_reloc_count(object) == 1);
    assert(archbfd_object_section(object, 0) !is null);
    assert(archbfd_object_symbol(object, 0) !is null);
    assert(archbfd_object_reloc(object, 0) !is null);

    // A document that does not exist is rejected and reported.
    assert(archbfd_spec_load_file(toStringz(args[1] ~ "/no-such.bfd.yaml"), 0, 0, &d) is null);
    assert(d.count > 0);
    assert(d.errors > 0);
    dsym_diags_free(&d);
    dsym_diags_init(&d);
    assert(d.count == 0);

    writeln("D ArchBFD interface tests passed");
    return 0;
}
