/*
 * test_archbfd — regression tests for the BFD object-format catalogue.
 *
 *   test_archbfd ARCHBFD_DIR
 *
 * 1. Every document loads cleanly and survives a round trip through YAML,
 *    JSON, XML and S-expressions with identical content.
 * 2. The known binutils facts hold: ELF header sizes, relocation numbers,
 *    the container inheritance of the emulation documents.
 * 3. Invalid documents are rejected with located diagnostics.
 * 4. The catalogue scans a directory, resolves names and caches documents.
 */
#include "ArchBFD.h"

#include <dirent.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, checks;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            failures++;                                                        \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);               \
            fprintf(stderr, __VA_ARGS__);                                      \
            fputc('\n', stderr);                                               \
        }                                                                      \
    } while (0)

static const DsymFormat FORMATS[] = {DSYM_FORMAT_YAML, DSYM_FORMAT_JSON, DSYM_FORMAT_XML,
                                     DSYM_FORMAT_SEXPR};

/* ---- 1. document round trips -------------------------------------------- */

static bool canonical(const ArchBFDSpec *s, DsymBuf *out) {
    DsymDiags d;
    dsym_diags_init(&d);
    bool ok = archbfd_spec_write(s, DSYM_FORMAT_JSON, DSYM_WRITE_COMPACT, out, &d);
    dsym_diags_free(&d);
    return ok;
}

static void roundtrip_document(const char *path) {
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFDSpec *s = archbfd_spec_load_file(path, DSYM_FORMAT_AUTO, 0, &d);
    CHECK(s != NULL, "load %s", path);
    CHECK(d.errors == 0, "%s: %zu error(s)", path, d.errors);
    if (d.count) dsym_diags_print(&d, stderr);
    dsym_diags_free(&d);
    if (!s) return;

    DsymBuf ref;
    dsym_buf_init(&ref);
    CHECK(canonical(s, &ref), "%s: canonical JSON", path);

    for (size_t f = 0; f < sizeof FORMATS / sizeof *FORMATS; f++) {
        DsymBuf text, again;
        dsym_buf_init(&text);
        dsym_buf_init(&again);
        dsym_diags_init(&d);
        bool ok = archbfd_spec_write(s, FORMATS[f], 0, &text, &d);
        CHECK(ok, "%s: write %s", path, dsym_format_name(FORMATS[f]));
        char vpath[1200];
        snprintf(vpath, sizeof vpath, "%s.roundtrip%s", path, dsym_format_extension(FORMATS[f]));
        ArchBFDSpec *s2 = ok ? archbfd_spec_load_buffer(text.data, text.len, FORMATS[f], vpath, 0, &d)
                             : NULL;
        CHECK(s2 != NULL, "%s: re-read %s", path, dsym_format_name(FORMATS[f]));
        if (!s2) dsym_diags_print(&d, stderr);
        if (s2) {
            CHECK(canonical(s2, &again), "%s: canonical (2)", path);
            CHECK(again.len == ref.len && memcmp(again.data, ref.data, ref.len) == 0,
                  "%s: %s round trip differs", path, dsym_format_name(FORMATS[f]));
            CHECK(s2->reloc_count == s->reloc_count, "%s: relocation count", path);
            CHECK(s2->record_count == s->record_count, "%s: record count", path);
            for (size_t i = 0; i < s2->reloc_count && i < s->reloc_count; i++) {
                CHECK(s2->relocations[i].number == s->relocations[i].number,
                      "%s: relocation %zu number", path, i);
                CHECK(s2->relocations[i].size == s->relocations[i].size, "%s: relocation %zu size",
                      path, i);
            }
        }
        archbfd_spec_free(s2);
        dsym_buf_free(&text);
        dsym_buf_free(&again);
        dsym_diags_free(&d);
    }
    dsym_buf_free(&ref);
    archbfd_spec_free(s);
}

static size_t roundtrip_dir(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "cannot open %s\n", dir);
        return 0;
    }
    char *names[256];
    size_t n = 0;
    struct dirent *de;
    while ((de = readdir(d))) {
        const char *m = strstr(de->d_name, ".bfd.");
        if (!m || de->d_name[0] == '.') continue;
        if (n < sizeof names / sizeof *names) names[n++] = strdup(de->d_name);
    }
    closedir(d);
    for (size_t i = 0; i < n; i++) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        roundtrip_document(path);
        free(names[i]);
    }
    return n;
}

/* ---- 2. binutils facts -------------------------------------------------- */

static ArchBFDSpec *load(const char *dir, const char *name) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.bfd.yaml", dir, name);
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFDSpec *s = archbfd_spec_load_file(path, DSYM_FORMAT_AUTO, 0, &d);
    if (!s) dsym_diags_print(&d, stderr);
    dsym_diags_free(&d);
    return s;
}

static void check_record(const ArchBFDSpec *s, const char *record, int64_t size,
                         size_t fields) {
    const ArchBFDRecord *r = archbfd_find_record(s, record);
    CHECK(r != NULL, "%s: record '%s' is missing", s->name ? s->name : "?", record);
    if (!r) return;
    CHECK(r->size == size, "%s: record '%s' is %" PRId64 " bytes, expected %" PRId64,
          s->name ? s->name : "?", record, r->size, size);
    CHECK(r->field_count == fields, "%s: record '%s' has %zu fields, expected %zu",
          s->name ? s->name : "?", record, r->field_count, fields);
}

static void facts(const char *dir) {
    /* ---- ELF64 ---- */
    ArchBFDSpec *s = load(dir, "ELF64");
    if (s) {
        CHECK(s->has_format && s->format == ARCHBFD_FMT_ELF, "ELF64 format");
        CHECK(s->has_byte_order && s->byte_order == ARCHBFD_BO_LITTLE, "ELF64 byte order");
        check_record(s, "ehdr", 64, 14);
        check_record(s, "phdr", 56, 8);
        check_record(s, "shdr", 64, 10);
        check_record(s, "sym", 24, 6);
        check_record(s, "rel", 16, 2);
        check_record(s, "rela", 24, 3);
        /* e_ident is 16 bytes, e_shoff sits at offset 40. */
        const ArchBFDRecord *ehdr = archbfd_find_record(s, "ehdr");
        if (ehdr && ehdr->field_count == 14) {
            CHECK(ehdr->fields[0].name && !strcmp(ehdr->fields[0].name, "e_ident"),
                  "ELF64 e_ident");
            CHECK(ehdr->fields[0].size == 16, "ELF64 e_ident is 16 bytes");
            CHECK(ehdr->fields[6].offset == 40, "ELF64 e_shoff is at offset 40");
        }
        /* r_info splits into the type and the symbol index. */
        const ArchBFDRecord *rela = archbfd_find_record(s, "rela");
        if (rela) {
            CHECK(rela->encoding_count == 2, "ELF64 rela has two encodings");
            if (rela->encoding_count == 2) {
                /* The value is shifted first and masked afterwards. */
                CHECK(rela->encodings[0].shift == 0 && rela->encodings[0].mask == 0xFFFFFFFFu,
                      "ELF64 rela type mask");
                CHECK(rela->encodings[1].shift == 32 && rela->encodings[1].mask == 0xFFFFFFFFu,
                      "ELF64 rela symbol mask");
            }
        }
        /* The x86-64 relocation numbers come from include/elf/x86-64.h. */
        int32_t i = archbfd_find_reloc(s, "R_X86_64_PC32");
        CHECK(i >= 0, "ELF64 has R_X86_64_PC32");
        if (i >= 0) {
            const ArchBFDReloc *r = &s->relocations[i];
            CHECK(r->number == 2, "R_X86_64_PC32 is number 2, got %" PRId64, r->number);
            CHECK(r->size == 4, "R_X86_64_PC32 is 4 bytes");
            CHECK(r->bitsize == 32, "R_X86_64_PC32 is 32 bits");
            CHECK(r->pcrel, "R_X86_64_PC32 is pc-relative");
            CHECK(r->overflow == ARCHBFD_OVF_SIGNED, "R_X86_64_PC32 overflow policy");
        }
        i = archbfd_find_reloc_number(s, 1);
        CHECK(i >= 0 && s->relocations[i].name && !strcmp(s->relocations[i].name, "R_X86_64_64"),
              "R_X86_64_64 is number 1");
        if (i >= 0) {
            CHECK(s->relocations[i].size == 8, "R_X86_64_64 is 8 bytes");
            CHECK(s->relocations[i].bitsize == 64, "R_X86_64_64 is 64 bits");
        }
        /* SHT_PROGBITS is 1, SHT_RELA is 4, SHT_SYMTAB is 2. */
        int64_t v = 0;
        CHECK(archbfd_constant(s, "section_types", "SHT_SYMTAB", &v) && v == 2,
              "SHT_SYMTAB is 2");
        CHECK(archbfd_constant(s, "section_types", "SHT_RELA", &v) && v == 4, "SHT_RELA is 4");
        CHECK(archbfd_constant(s, "section_types", "SHT_REL", &v) && v == 9, "SHT_REL is 9");
        CHECK(archbfd_constant(s, "machine_types", "EM_X86_64", &v) && v == 62,
              "EM_X86_64 is 62");
        CHECK(!archbfd_constant(s, "section_types", "SHT_NOPE", &v), "unknown constant");
        archbfd_spec_free(s);
    }

    /* ---- ELF32 ---- */
    s = load(dir, "ELF32");
    if (s) {
        check_record(s, "ehdr", 52, 14);
        check_record(s, "shdr", 40, 10);
        check_record(s, "sym", 16, 6);
        check_record(s, "rel", 8, 2);
        check_record(s, "rela", 12, 3);
        const ArchBFDRecord *rela = archbfd_find_record(s, "rela");
        if (rela && rela->encoding_count == 2) {
            CHECK(rela->encodings[0].mask == 0xFF, "ELF32 rela type is one byte");
            CHECK(rela->encodings[1].shift == 8, "ELF32 rela symbol starts at bit 8");
            CHECK(rela->encodings[1].mask == 0xFFFFFF, "ELF32 rela symbol is 24 bits");
        }
        int32_t i = archbfd_find_reloc(s, "R_386_PC32");
        CHECK(i >= 0 && s->relocations[i].number == 2, "R_386_PC32 is number 2");
        archbfd_spec_free(s);
    }

    /* ---- hppa and m68k inherit the ELF32 container ---- */
    s = load(dir, "hppa");
    if (s) {
        CHECK(s->container && !strcmp(s->container, "ELF32"), "hppa inherits from ELF32");
        CHECK(s->has_byte_order && s->byte_order == ARCHBFD_BO_BIG, "hppa is big endian");
        int32_t i = archbfd_find_reloc(s, "R_PARISC_DIR32");
        CHECK(i >= 0, "hppa has R_PARISC_DIR32");
        if (i >= 0) {
            CHECK(s->relocations[i].number == 1, "R_PARISC_DIR32 is number 1");
            CHECK(s->relocations[i].size == 4, "R_PARISC_DIR32 is 4 bytes");
            CHECK(s->relocations[i].bitsize == 32, "R_PARISC_DIR32 is 32 bits");
        }
        archbfd_spec_free(s);
    }
    s = load(dir, "m68k");
    if (s) {
        CHECK(s->container && !strcmp(s->container, "ELF32"), "m68k inherits from ELF32");
        int32_t i = archbfd_find_reloc(s, "R_68K_PC32");
        CHECK(i >= 0 && s->relocations[i].number == 4, "R_68K_PC32 is number 4");
        if (i >= 0) CHECK(s->relocations[i].pcrel, "R_68K_PC32 is pc-relative");
        archbfd_spec_free(s);
    }

    /* ---- COFF ---- */
    s = load(dir, "COFF");
    if (s) {
        check_record(s, "filehdr", 20, 7);
        check_record(s, "scnhdr", 40, 10);
        check_record(s, "syment", 18, 6);
        check_record(s, "reloc", 10, 3);
        int32_t i = archbfd_find_reloc(s, "dir32");
        CHECK(i >= 0, "COFF has the dir32 relocation");
        if (i >= 0) {
            CHECK(s->relocations[i].size == 4, "COFF dir32 is 4 bytes");
            CHECK(s->relocations[i].bitsize == 32, "COFF dir32 is 32 bits");
        }
        archbfd_spec_free(s);
    }

    /* ---- PE ---- */
    s = load(dir, "PE");
    if (s) {
        CHECK(s->container && !strcmp(s->container, "COFF"), "PE inherits from COFF");
        check_record(s, "optional_header", 96, 30);
        check_record(s, "optional_header_64", 112, 29);
        check_record(s, "data_directory", 8, 2);
        int64_t v = 0;
        CHECK(archbfd_constant(s, "machine_types", "IMAGE_FILE_MACHINE_AMD64", &v) && v == 0x8664,
              "IMAGE_FILE_MACHINE_AMD64 is 0x8664");
        archbfd_spec_free(s);
    }

    /* ---- Mach-O ---- */
    s = load(dir, "MachO");
    if (s) {
        check_record(s, "header", 28, 7);
        check_record(s, "header_64", 32, 8);
        check_record(s, "load_command", 8, 2);
        check_record(s, "reloc", 8, 2);
        int64_t v = 0;
        CHECK(archbfd_constant(s, "load_commands", "BFD_MACH_O_LC_SEGMENT", &v) && v == 1,
              "BFD_MACH_O_LC_SEGMENT is 1");
        archbfd_spec_free(s);
    }

    /* ---- ECOFF / IRIX ---- */
    s = load(dir, "irix");
    if (s) {
        check_record(s, "filehdr", 20, 7);
        check_record(s, "reloc", 8, 2);
        int32_t i = archbfd_find_reloc(s, "REFWORD");
        CHECK(i >= 0, "irix has the REFWORD relocation");
        if (i >= 0) {
            CHECK(s->relocations[i].number == 2, "REFWORD is number 2");
            CHECK(s->relocations[i].size == 4, "REFWORD is 4 bytes");
        }
        archbfd_spec_free(s);
    }
}

/* ---- 3. diagnostics ----------------------------------------------------- */

static void expect_error(const char *text, const char *needle, int64_t line) {
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFDSpec *s = archbfd_spec_load_buffer(text, strlen(text), DSYM_FORMAT_YAML, "test.yaml", 0,
                                               &d);
    CHECK(s == NULL, "expected a rejection: %s", needle);
    CHECK(d.errors > 0, "expected an error for '%s'", needle);
    bool found = false;
    for (size_t i = 0; i < d.count; i++)
        if (d.items[i].severity == DSYM_ERROR && strstr(d.items[i].msg, needle)) {
            found = true;
            if (line) CHECK(d.items[i].line == (uint32_t)line, "error at line %u, expected %" PRId64,
                            d.items[i].line, line);
        }
    CHECK(found, "no diagnostic mentions '%s'", needle);
    if (!found) dsym_diags_print(&d, stderr);
    archbfd_spec_free(s);
    dsym_diags_free(&d);
}

/* Unknown keys are reported as warnings, not errors. */
static void expect_warning(const char *text, const char *needle) {
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFDSpec *s = archbfd_spec_load_buffer(text, strlen(text), DSYM_FORMAT_YAML, "test.yaml", 0,
                                               &d);
    CHECK(s != NULL, "expected the document to load");
    bool found = false;
    for (size_t i = 0; i < d.count; i++)
        if (d.items[i].severity == DSYM_WARNING && strstr(d.items[i].msg, needle)) found = true;
    CHECK(found, "no warning mentions '%s'", needle);
    archbfd_spec_free(s);
    dsym_diags_free(&d);
}

static void diagnostics(void) {
    expect_error("schema: 2\nname: x\n", "schema must be 1", 0);
    expect_error("schema: 1\n", "missing required key 'name'", 0);
    expect_error("schema: 1\nname: x\nformat: elfx\n", "unknown object format", 0);
    expect_error("schema: 1\nname: x\nbyte_order: sideways\n", "unknown byte order", 0);
    expect_error("schema: 1\nname: x\nmagic: \"zz\"\n", "hexadecimal", 0);
    expect_error("schema: 1\nname: x\nmagic: \"abc\"\n", "even number", 0);
    expect_error("schema: 1\nname: x\nrecords:\n  - {name: r, size: 2, fields: "
                 "[{name: f, offset: 0, size: 4}]}\n",
                 "but the record is 2 bytes", 0);
    expect_error("schema: 1\nname: x\nrelocations:\n  - {name: a, number: 1, size: 4, "
                 "bitsize: 64}\n",
                 "claims 64 bits", 0);
    expect_error("schema: 1\nname: x\nrelocations:\n  - {name: a, number: 1}\n  - {name: b, "
                 "number: 1}\n",
                 "share the number 1", 0);
    expect_error("schema: 1\nname: x\nrecords:\n  - {name: r}\n  - {name: r}\n",
                 "duplicate record 'r'", 0);
    expect_error("schema: 1\nname: x\nrecords:\n  - {name: r, encodings: [{field: nope, "
                 "target: type, mask: 1}]}\n",
                 "unknown field 'nope'", 0);
    expect_error("schema: 1\nname: \"has space\"\n", "printable ASCII", 0);
    expect_warning("schema: 1\nname: x\nunknown_key: 1\n", "unknown key 'unknown_key'");
}

/* ---- 4. catalogue ------------------------------------------------------- */

static void catalogue(const char *dir, size_t expected) {
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFD *db = archbfd_open(dir, &d);
    CHECK(db != NULL, "open catalogue directory");
    if (d.count) dsym_diags_print(&d, stderr);
    if (!db) {
        dsym_diags_free(&d);
        return;
    }
    CHECK(archbfd_count(db) == expected, "catalogue has %zu entries, directory %zu",
          archbfd_count(db), expected);
    for (size_t i = 0; i < archbfd_count(db); i++) {
        const ArchBFDEntry *e = archbfd_entry(db, i);
        CHECK(e && e->name && e->path, "entry %zu", i);
        if (!e) continue;
        const ArchBFDSpec *s = archbfd_load(db, e, 0, &d);
        CHECK(s != NULL, "load %s", e->name);
        if (s) {
            CHECK(e->format && !strcmp(e->format, archbfd_format_name(s->format)),
                  "%s: summary format is stale", e->name);
            CHECK(archbfd_load(db, e, 0, &d) == s, "%s: cache", e->name);
        }
    }
    const ArchBFDEntry *e = archbfd_find(db, "ELF64");
    CHECK(e && !strcmp(e->name, "ELF64"), "find ELF64");
    e = archbfd_find(db, "elf64-x86-64");
    CHECK(e && !strcmp(e->name, "ELF64"), "find by target vector");
    CHECK(archbfd_find(db, "no-such-format") == NULL, "find missing");
    const ArchBFDSpec *s = archbfd_load_name(db, "m68k", 0, &d);
    CHECK(s != NULL, "load m68k by name");
    archbfd_close(db);
    dsym_diags_free(&d);
}

/* ---- 4. object reader ---------------------------------------------------- */

/* A minimal ELF64 object: five sections, three symbols and one
   relocation.  Building it here keeps the test independent of a
   toolchain. */
static size_t build_elf64(uint8_t *buf, size_t cap) {
    memset(buf, 0, cap);
    const size_t ehdr_size = 64, shdr_size = 64, sym_size = 24, rela_size = 24;
    const size_t nsec = 6;
    const size_t shoff = ehdr_size;
    const size_t text_off = shoff + nsec * shdr_size;
    const size_t text_size = 16;
    const size_t rela_off = text_off + text_size;
    const size_t symtab_off = rela_off + rela_size;
    const size_t nsym = 3;
    const size_t symtab_size = nsym * sym_size;
    const size_t strtab_off = symtab_off + symtab_size;
    /* "\0g\0f\0.text\0" */
    static const char strtab[] = "\0g\0f\0.text";
    const size_t strtab_size = sizeof strtab;
    const size_t shstr_off = strtab_off + strtab_size;
    static const char shstr[] = "\0.text\0.symtab\0.strtab\0.rela.text\0.shstrtab";
    const size_t shstr_size = sizeof shstr;
    const size_t total = shstr_off + shstr_size;
    if (total > cap) return 0;

    uint8_t *p = buf;
    memcpy(p, "\x7f" "ELF", 4);
    p[4] = 2; /* ELFCLASS64 */
    p[5] = 1; /* little endian */
    p[6] = 1; /* EV_CURRENT */
    memcpy(p + 16, "\x01\x00", 2);        /* e_type = ET_REL */
    memcpy(p + 18, "\x3e\x00", 2);        /* e_machine = EM_X86_64 */
    memcpy(p + 20, "\x01\x00\x00\x00", 4); /* e_version */
    memcpy(p + 40, &shoff, 8);
    memcpy(p + 52, &ehdr_size, 2);
    memcpy(p + 58, &shdr_size, 2);
    memcpy(p + 60, &nsec, 2);
    uint16_t shstrndx = 5;
    memcpy(p + 62, &shstrndx, 2); /* e_shstrndx */

    uint8_t *sh = buf + shoff;
    /* 0: the null section */
    /* 1: .text */
    uint32_t name = 1;
    memcpy(sh + 1 * shdr_size, &name, 4);
    uint32_t type = 1; /* SHT_PROGBITS */
    memcpy(sh + 1 * shdr_size + 4, &type, 4);
    uint64_t flags = 6;
    memcpy(sh + 1 * shdr_size + 8, &flags, 8);
    memcpy(sh + 1 * shdr_size + 24, &text_off, 8);
    memcpy(sh + 1 * shdr_size + 32, &text_size, 8);
    /* 2: .symtab */
    name = 7;
    memcpy(sh + 2 * shdr_size, &name, 4);
    type = 2; /* SHT_SYMTAB */
    memcpy(sh + 2 * shdr_size + 4, &type, 4);
    memcpy(sh + 2 * shdr_size + 24, &symtab_off, 8);
    memcpy(sh + 2 * shdr_size + 32, &symtab_size, 8);
    uint32_t link = 3, info = 1;
    memcpy(sh + 2 * shdr_size + 40, &link, 4);
    memcpy(sh + 2 * shdr_size + 44, &info, 4);
    uint64_t entsize = sym_size;
    memcpy(sh + 2 * shdr_size + 56, &entsize, 8);
    /* 3: .strtab */
    name = 15;
    memcpy(sh + 3 * shdr_size, &name, 4);
    type = 3; /* SHT_STRTAB */
    memcpy(sh + 3 * shdr_size + 4, &type, 4);
    memcpy(sh + 3 * shdr_size + 24, &strtab_off, 8);
    memcpy(sh + 3 * shdr_size + 32, &strtab_size, 8);
    /* 4: .rela.text */
    name = 23;
    memcpy(sh + 4 * shdr_size, &name, 4);
    type = 4; /* SHT_RELA */
    memcpy(sh + 4 * shdr_size + 4, &type, 4);
    memcpy(sh + 4 * shdr_size + 24, &rela_off, 8);
    memcpy(sh + 4 * shdr_size + 32, &rela_size, 8);
    link = 3;
    info = 1;
    memcpy(sh + 4 * shdr_size + 40, &link, 4);
    memcpy(sh + 4 * shdr_size + 44, &info, 4);
    memcpy(sh + 4 * shdr_size + 56, &entsize, 8);
    /* 5: .shstrtab */
    name = 34;
    memcpy(sh + 5 * shdr_size, &name, 4);
    type = 3; /* SHT_STRTAB */
    memcpy(sh + 5 * shdr_size + 4, &type, 4);
    memcpy(sh + 5 * shdr_size + 24, &shstr_off, 8);
    memcpy(sh + 5 * shdr_size + 32, &shstr_size, 8);

    /* The symbol table: the null symbol, "g" and "f". */
    uint8_t *sym = buf + symtab_off;
    uint32_t st_name = 1;
    memcpy(sym + 1 * sym_size, &st_name, 4);
    uint8_t st_info = 0x11; /* global object */
    sym[1 * sym_size + 4] = st_info;
    uint16_t st_shndx = 1;
    memcpy(sym + 1 * sym_size + 6, &st_shndx, 2);
    uint64_t st_value = 0, st_size = 4;
    memcpy(sym + 1 * sym_size + 8, &st_value, 8);
    memcpy(sym + 1 * sym_size + 16, &st_size, 8);
    st_name = 3;
    memcpy(sym + 2 * sym_size, &st_name, 4);
    st_info = 0x12; /* global function */
    sym[2 * sym_size + 4] = st_info;
    memcpy(sym + 2 * sym_size + 6, &st_shndx, 2);
    st_value = 0;
    st_size = 16;
    memcpy(sym + 2 * sym_size + 8, &st_value, 8);
    memcpy(sym + 2 * sym_size + 16, &st_size, 8);

    /* One relocation: R_X86_64_PC32 (2) against the symbol "g" (1). */
    uint8_t *rel = buf + rela_off;
    uint64_t r_offset = 8, r_info = ((uint64_t)1 << 32) | 2, r_addend = -4;
    memcpy(rel, &r_offset, 8);
    memcpy(rel + 8, &r_info, 8);
    memcpy(rel + 16, &r_addend, 8);

    memcpy(buf + strtab_off, strtab, strtab_size);
    memcpy(buf + shstr_off, shstr, shstr_size);
    return total;
}

static void object_reader(const char *dir, const char *write_path) {
    uint8_t buf[4096];
    size_t len = build_elf64(buf, sizeof buf);
    CHECK(len > 0, "build the test object");
    if (write_path && len) {
        FILE *fh = fopen(write_path, "wb");
        CHECK(fh != NULL, "write %s", write_path);
        if (fh) {
            CHECK(fwrite(buf, 1, len, fh) == len, "write the test object");
            fclose(fh);
        }
    }
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFDSpec *s = load(dir, "ELF64");
    if (!s) {
        dsym_diags_free(&d);
        return;
    }
    ArchBFDObject *obj = archbfd_object_open_memory(buf, len, s, &d);
    CHECK(obj != NULL, "read the test object");
    if (!obj) {
        if (d.count) dsym_diags_print(&d, stderr);
        archbfd_spec_free(s);
        dsym_diags_free(&d);
        return;
    }
    CHECK(!strcmp(archbfd_object_kind(obj), "elf64"), "object kind is elf64");
    CHECK(archbfd_object_machine(obj) == 62, "machine is EM_X86_64");
    CHECK(archbfd_object_section_count(obj) == 6, "%zu sections",
          archbfd_object_section_count(obj));
    if (archbfd_object_section_count(obj) == 6) {
        const ArchBFDObjectSection *sec = archbfd_object_section(obj, 1);
        CHECK(sec && !strcmp(sec->name, ".text"), "section 1 is .text");
        CHECK(sec && sec->type == 1, "section 1 is SHT_PROGBITS");
        CHECK(sec && sec->size == 16, "section 1 is 16 bytes");
        const ArchBFDObjectSection *symsec = archbfd_object_section(obj, 2);
        CHECK(symsec && !strcmp(symsec->name, ".symtab"), "section 2 is .symtab");
        const ArchBFDObjectSection *rela = archbfd_object_section(obj, 4);
        CHECK(rela && !strcmp(rela->name, ".rela.text"), "section 4 is .rela.text");
    }
    CHECK(archbfd_object_symbol_count(obj) == 3, "%zu symbols",
          archbfd_object_symbol_count(obj));
    if (archbfd_object_symbol_count(obj) == 3) {
        const ArchBFDObjectSymbol *sym = archbfd_object_symbol(obj, 1);
        CHECK(sym && !strcmp(sym->name, "g"), "symbol 1 is g");
        CHECK(sym && sym->size == 4, "symbol 1 is 4 bytes");
        CHECK(sym && sym->bind && !strcmp(sym->bind, "STB_GLOBAL"), "symbol 1 is global");
        CHECK(sym && sym->type && !strcmp(sym->type, "STT_OBJECT"), "symbol 1 is an object");
        const ArchBFDObjectSymbol *fn = archbfd_object_symbol(obj, 2);
        CHECK(fn && !strcmp(fn->name, "f"), "symbol 2 is f");
        CHECK(fn && fn->type && !strcmp(fn->type, "STT_FUNC"), "symbol 2 is a function");
    }
    CHECK(archbfd_object_reloc_count(obj) == 1, "%zu relocations",
          archbfd_object_reloc_count(obj));
    if (archbfd_object_reloc_count(obj) == 1) {
        const ArchBFDObjectReloc *rel = archbfd_object_reloc(obj, 0);
        CHECK(rel && rel->type == 2, "the relocation type is 2");
        CHECK(rel && rel->type_name && !strcmp(rel->type_name, "R_X86_64_PC32"),
              "the relocation is R_X86_64_PC32");
        CHECK(rel && rel->symbol == 1, "the relocation uses symbol 1");
        CHECK(rel && rel->symbol_name && !strcmp(rel->symbol_name, "g"),
              "the relocation points at g");
        CHECK(rel && rel->offset == 8, "the relocation is at offset 8");
        CHECK(rel && rel->addend == -4, "the addend is -4");
    }
    archbfd_object_close(obj);

    /* An object that does not match the description is rejected. */
    DsymDiags bad;
    dsym_diags_init(&bad);
    uint8_t broken[4096];
    size_t blen = build_elf64(broken, sizeof broken);
    broken[4] = 1; /* claim ELFCLASS32 */
    ArchBFDObject *obj2 = archbfd_object_open_memory(broken, blen, s, &bad);
    CHECK(obj2 == NULL, "a mismatching class is rejected");
    CHECK(bad.errors > 0, "the rejection is reported");
    archbfd_object_close(obj2);
    dsym_diags_free(&bad);

    archbfd_spec_free(s);
    dsym_diags_free(&d);
}

/* A minimal COFF object: one section, two symbols and one relocation. */
static size_t build_coff(uint8_t *buf, size_t cap) {
    memset(buf, 0, cap);
    const size_t filehdr_size = 20, scnhdr_size = 40, syment_size = 18, reloc_size = 10;
    const size_t text_off = filehdr_size + scnhdr_size;
    const size_t text_size = 10;
    const size_t reloc_off = text_off + text_size;
    const size_t symtab_off = reloc_off + reloc_size;
    const size_t nsym = 2;
    const size_t total = symtab_off + nsym * syment_size;
    if (total > cap) return 0;

    uint8_t *p = buf;
    memcpy(p, "\x4c\x01", 2); /* f_magic = I386MAGIC */
    uint16_t nscns = 1;
    memcpy(p + 2, &nscns, 2);
    uint32_t symptr = (uint32_t)symtab_off, nsyms = (uint32_t)nsym;
    memcpy(p + 8, &symptr, 4);
    memcpy(p + 12, &nsyms, 4);

    uint8_t *sh = buf + filehdr_size;
    memcpy(sh, ".text", 5);
    uint32_t vaddr = 0, size = (uint32_t)text_size, scnptr = (uint32_t)text_off;
    uint32_t relptr = (uint32_t)reloc_off, lnnoptr = 0, flags = 0x20;
    memcpy(sh + 12, &vaddr, 4);
    memcpy(sh + 16, &size, 4);
    memcpy(sh + 20, &scnptr, 4);
    memcpy(sh + 24, &relptr, 4);
    memcpy(sh + 28, &lnnoptr, 4);
    uint16_t nreloc = 1, nlnno = 0;
    memcpy(sh + 32, &nreloc, 2);
    memcpy(sh + 34, &nlnno, 2);
    memcpy(sh + 36, &flags, 4);

    /* The relocation: R_DIR32 (6) against the symbol "f" (1). */
    uint8_t *rel = buf + reloc_off;
    uint32_t r_vaddr = 4, r_symndx = 1;
    uint16_t r_type = 6;
    memcpy(rel, &r_vaddr, 4);
    memcpy(rel + 4, &r_symndx, 4);
    memcpy(rel + 8, &r_type, 2);

    /* The symbol table: the null symbol and "f" with an inline name. */
    uint8_t *sym = buf + symtab_off;
    memcpy(sym + syment_size, "f", 2);
    uint32_t e_value = 0;
    uint16_t e_scnum = 1, e_type = 0x20; /* function */
    uint8_t e_sclass = 2;                 /* C_EXT */
    memcpy(sym + syment_size + 8, &e_value, 4);
    memcpy(sym + syment_size + 12, &e_scnum, 2);
    memcpy(sym + syment_size + 14, &e_type, 2);
    sym[syment_size + 16] = e_sclass;
    return total;
}

static void coff_reader(const char *dir) {
    uint8_t buf[1024];
    size_t len = build_coff(buf, sizeof buf);
    CHECK(len > 0, "build the test COFF object");
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFDSpec *s = load(dir, "COFF");
    if (!s) {
        dsym_diags_free(&d);
        return;
    }
    ArchBFDObject *obj = archbfd_object_open_memory(buf, len, s, &d);
    CHECK(obj != NULL, "read the test COFF object");
    if (!obj) {
        if (d.count) dsym_diags_print(&d, stderr);
        archbfd_spec_free(s);
        dsym_diags_free(&d);
        return;
    }
    CHECK(!strcmp(archbfd_object_kind(obj), "coff"), "object kind is coff");
    CHECK(archbfd_object_section_count(obj) == 1, "%zu sections",
          archbfd_object_section_count(obj));
    if (archbfd_object_section_count(obj) == 1) {
        const ArchBFDObjectSection *sec = archbfd_object_section(obj, 0);
        CHECK(sec && !strcmp(sec->name, ".text"), "section 0 is .text");
        CHECK(sec && sec->size == 10, "section 0 is 10 bytes");
        CHECK(sec && sec->rel_count == 1, "section 0 has one relocation");
    }
    CHECK(archbfd_object_symbol_count(obj) == 2, "%zu symbols",
          archbfd_object_symbol_count(obj));
    if (archbfd_object_symbol_count(obj) == 2) {
        const ArchBFDObjectSymbol *sym = archbfd_object_symbol(obj, 1);
        CHECK(sym && !strcmp(sym->name, "f"), "symbol 1 is f");
        CHECK(sym && sym->shndx == 1, "symbol 1 is in section 1");
        CHECK(sym && sym->class_value == 2, "symbol 1 is C_EXT");
    }
    CHECK(archbfd_object_reloc_count(obj) == 1, "%zu relocations",
          archbfd_object_reloc_count(obj));
    if (archbfd_object_reloc_count(obj) == 1) {
        const ArchBFDObjectReloc *rel = archbfd_object_reloc(obj, 0);
        CHECK(rel && rel->type == 6, "the relocation type is 6 (R_DIR32)");
        CHECK(rel && rel->type_name && !strcmp(rel->type_name, "dir32"),
              "the relocation is dir32");
        CHECK(rel && rel->symbol == 1, "the relocation uses symbol 1");
        CHECK(rel && rel->symbol_name && !strcmp(rel->symbol_name, "f"),
              "the relocation points at f");
        CHECK(rel && rel->offset == 4, "the relocation is at offset 4");
    }
    archbfd_object_close(obj);
    archbfd_spec_free(s);
    dsym_diags_free(&d);
}

/* A minimal PE image: a DOS stub, the NT headers, one section, two symbols
   and one relocation. */
static size_t build_pe(uint8_t *buf, size_t cap) {
    memset(buf, 0, cap);
    const size_t lfanew = 0x80;
    const size_t scnhdr_size = 40, syment_size = 18, reloc_size = 10;
    const size_t coff_hdr = lfanew + 4;   /* after the "PE\0\0" signature */
    const size_t opt_hdr = 224;           /* a PE32 optional header */
    const size_t sh_off = coff_hdr + 20 + opt_hdr;
    const size_t text_off = sh_off + scnhdr_size;
    const size_t text_size = 10;
    const size_t reloc_off = text_off + text_size;
    const size_t symtab_off = reloc_off + reloc_size;
    const size_t nsym = 2;
    const size_t total = symtab_off + nsym * syment_size;
    if (total > cap) return 0;

    uint8_t *p = buf;
    memcpy(p, "MZ", 2);                            /* e_magic */
    uint32_t lf = (uint32_t)lfanew;
    memcpy(p + 60, &lf, 4);                        /* e_lfanew */
    memcpy(p + lfanew, "PE\0\0", 4);               /* nt_signature */
    uint8_t *coff = buf + coff_hdr;
    memcpy(coff, "\x4c\x01", 2);                   /* f_magic = I386MAGIC */
    uint16_t nscns = 1, opthdr = (uint16_t)opt_hdr;
    memcpy(coff + 2, &nscns, 2);
    uint32_t symptr = (uint32_t)symtab_off, nsyms = (uint32_t)nsym;
    memcpy(coff + 8, &symptr, 4);
    memcpy(coff + 12, &nsyms, 4);
    memcpy(coff + 16, &opthdr, 2);

    uint8_t *sh = buf + sh_off;
    memcpy(sh, ".text", 5);
    uint32_t vaddr = 0, size = (uint32_t)text_size, scnptr = (uint32_t)text_off;
    uint32_t relptr = (uint32_t)reloc_off, lnnoptr = 0, flags = 0x60000020;
    memcpy(sh + 12, &vaddr, 4);
    memcpy(sh + 16, &size, 4);
    memcpy(sh + 20, &scnptr, 4);
    memcpy(sh + 24, &relptr, 4);
    memcpy(sh + 28, &lnnoptr, 4);
    uint16_t nreloc = 1, nlnno = 0;
    memcpy(sh + 32, &nreloc, 2);
    memcpy(sh + 34, &nlnno, 2);
    memcpy(sh + 36, &flags, 4);

    uint8_t *rel = buf + reloc_off;
    uint32_t r_vaddr = 4, r_symndx = 1;
    uint16_t r_type = 6;
    memcpy(rel, &r_vaddr, 4);
    memcpy(rel + 4, &r_symndx, 4);
    memcpy(rel + 8, &r_type, 2);

    uint8_t *sym = buf + symtab_off;
    memcpy(sym + syment_size, "f", 2);
    uint16_t e_scnum = 1, e_type = 0x20;
    uint8_t e_sclass = 2;
    memcpy(sym + syment_size + 12, &e_scnum, 2);
    memcpy(sym + syment_size + 14, &e_type, 2);
    sym[syment_size + 16] = e_sclass;
    return total;
}

static void pe_reader(const char *dir) {
    uint8_t buf[2048];
    size_t len = build_pe(buf, sizeof buf);
    CHECK(len > 0, "build the test PE image");
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFDSpec *s = load(dir, "PE");
    if (!s) {
        dsym_diags_free(&d);
        return;
    }
    ArchBFDObject *obj = archbfd_object_open_memory(buf, len, s, &d);
    CHECK(obj != NULL, "read the test PE image");
    if (!obj) {
        if (d.count) dsym_diags_print(&d, stderr);
        archbfd_spec_free(s);
        dsym_diags_free(&d);
        return;
    }
    CHECK(!strcmp(archbfd_object_kind(obj), "pe"), "object kind is pe");
    CHECK(archbfd_object_section_count(obj) == 1, "%zu sections",
          archbfd_object_section_count(obj));
    if (archbfd_object_section_count(obj) == 1) {
        const ArchBFDObjectSection *sec = archbfd_object_section(obj, 0);
        CHECK(sec && !strcmp(sec->name, ".text"), "section 0 is .text");
        CHECK(sec && sec->size == 10, "section 0 is 10 bytes");
    }
    CHECK(archbfd_object_symbol_count(obj) == 2, "%zu symbols",
          archbfd_object_symbol_count(obj));
    if (archbfd_object_symbol_count(obj) == 2) {
        const ArchBFDObjectSymbol *sym = archbfd_object_symbol(obj, 1);
        CHECK(sym && !strcmp(sym->name, "f"), "symbol 1 is f");
    }
    CHECK(archbfd_object_reloc_count(obj) == 1, "%zu relocations",
          archbfd_object_reloc_count(obj));
    archbfd_object_close(obj);
    archbfd_spec_free(s);
    dsym_diags_free(&d);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: test_archbfd ARCHBFD_DIR [OBJECT_OUT]\n");
        return 2;
    }
    size_t n = roundtrip_dir(argv[1]);
    CHECK(n > 0, "no documents found");
    facts(argv[1]);
    diagnostics();
    object_reader(argv[1], argc > 2 ? argv[2] : NULL);
    coff_reader(argv[1]);
    pe_reader(argv[1]);
    catalogue(argv[1], n);
    printf("%d check(s), %d failure(s), %zu document(s) x %zu formats\n", checks, failures, n,
           sizeof FORMATS / sizeof *FORMATS);
    return failures ? 1 : 0;
}
