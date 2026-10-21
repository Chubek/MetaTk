/*
 * archbfd — command-line front end for the MetaArch BFD object-format
 * catalogue.
 *
 *   archbfd validate [--strict] [--lint] [-i FMT] FILE...
 *   archbfd convert  -f FORMAT [-o OUT] [-i FMT] [--compact] FILE
 *   archbfd list     [CATALOGUE|DIR]
 *   archbfd catalogue DIR [-o OUT] [-f FMT] [--root ROOT]
 *   archbfd info     [-i FMT] FILE
 *   archbfd relocs   [-i FMT] FILE [NAME]
 *   archbfd records  [-i FMT] FILE [NAME]
 *   archbfd object   [-f FORMAT] OBJECT
 *
 * FORMAT is yaml, json, xml or sexpr.  Input format follows the extension
 * (override with -i FORMAT).  Exit status: 0 ok, 1 invalid input, 2 usage.
 */
#include "ArchBFD.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *f) {
    fputs("usage:\n"
          "  archbfd validate [--strict] [--lint] [-i FMT] FILE...\n"
          "  archbfd convert -f FMT [-o OUT] [-i FMT] [--compact] FILE\n"
          "  archbfd list [CATALOGUE|DIR]\n"
          "  archbfd catalogue DIR [-o OUT] [-f FMT] [--root ROOT]\n"
          "  archbfd info [-i FMT] FILE\n"
          "  archbfd relocs [-i FMT] FILE [NAME]\n"
          "  archbfd records [-i FMT] FILE [NAME]\n"
          "  archbfd object [-f FORMAT] OBJECT\n"
          "formats: yaml json xml sexpr\n",
          f);
}

static int finish(DsymDiags *d, int rc) {
    dsym_diags_print(d, stderr);
    dsym_diags_free(d);
    return rc;
}

static bool parse_fmt(const char *s, DsymFormat *f) {
    *f = dsym_format_from_name(s);
    if (*f == DSYM_FORMAT_AUTO) {
        fprintf(stderr, "archbfd: unknown format '%s'\n", s ? s : "");
        return false;
    }
    return true;
}

static int write_out(const DsymBuf *b, const char *out) {
    if (!out || !strcmp(out, "-")) {
        fwrite(b->data ? b->data : "", 1, b->len, stdout);
        return 0;
    }
    if (!dsym_buf_write_file(b, out)) {
        perror(out);
        return 1;
    }
    return 0;
}

static int cmd_validate(int argc, char **argv) {
    uint32_t flags = 0;
    DsymFormat in = DSYM_FORMAT_AUTO;
    int files = 0, bad = 0;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--strict"))
            flags |= ARCHBFD_LOAD_STRICT;
        else if (!strcmp(argv[i], "--lint"))
            flags |= ARCHBFD_LOAD_LINT;
        else if (!strcmp(argv[i], "-i") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &in)) return 2;
        } else {
            DsymDiags d;
            dsym_diags_init(&d);
            ArchBFDSpec *s = archbfd_spec_load_file(argv[i], in, flags, &d);
            files++;
            if (!s)
                bad++;
            else
                printf("%s: ok (%zu records, %zu relocations, %zu constant groups)\n", argv[i],
                       s->record_count, s->reloc_count, s->constant_group_count);
            archbfd_spec_free(s);
            dsym_diags_print(&d, stderr);
            dsym_diags_free(&d);
        }
    }
    if (!files) {
        usage(stderr);
        return 2;
    }
    return bad ? 1 : 0;
}

static int cmd_convert(int argc, char **argv) {
    DsymFormat in = DSYM_FORMAT_AUTO, outf = DSYM_FORMAT_AUTO;
    uint32_t wflags = 0;
    const char *out = NULL, *file = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-f") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &outf)) return 2;
        } else if (!strcmp(argv[i], "-i") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &in)) return 2;
        } else if (!strcmp(argv[i], "-o") && i + 1 < argc)
            out = argv[++i];
        else if (!strcmp(argv[i], "--compact"))
            wflags |= DSYM_WRITE_COMPACT;
        else if (!file)
            file = argv[i];
        else {
            usage(stderr);
            return 2;
        }
    }
    if (outf == DSYM_FORMAT_AUTO && out) outf = dsym_format_from_path(out);
    if (!file || outf == DSYM_FORMAT_AUTO) {
        usage(stderr);
        return 2;
    }
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFDSpec *s = archbfd_spec_load_file(file, in, 0, &d);
    if (!s) return finish(&d, 1);
    DsymBuf b;
    dsym_buf_init(&b);
    int rc = archbfd_spec_write(s, outf, wflags, &b, &d) ? write_out(&b, out) : 1;
    dsym_buf_free(&b);
    archbfd_spec_free(s);
    return finish(&d, rc);
}

static int cmd_list(int argc, char **argv) {
    const char *path = argc > 0 ? argv[0] : "ArchBFDCatalogue.yaml";
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFD *db = archbfd_open(path, &d);
    if (!db) return finish(&d, 1);
    printf("%-10s %-8s %-8s %-12s %s\n", "NAME", "FORMAT", "ORDER", "TARGET", "FILE");
    for (size_t i = 0; i < archbfd_count(db); i++) {
        const ArchBFDEntry *e = archbfd_entry(db, i);
        printf("%-10s %-8s %-8s %-12s %s\n", e->name, e->format ? e->format : "-",
               e->byte_order ? e->byte_order : "-", e->target_vector ? e->target_vector : "-",
               e->path);
    }
    archbfd_close(db);
    return finish(&d, 0);
}

static int cmd_catalogue(int argc, char **argv) {
    const char *dir = NULL, *out = NULL, *root = NULL;
    DsymFormat fmt = DSYM_FORMAT_AUTO;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc)
            out = argv[++i];
        else if (!strcmp(argv[i], "--root") && i + 1 < argc)
            root = argv[++i];
        else if (!strcmp(argv[i], "-f") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &fmt)) return 2;
        } else
            dir = argv[i];
    }
    if (!dir) {
        usage(stderr);
        return 2;
    }
    if (fmt == DSYM_FORMAT_AUTO) fmt = out ? dsym_format_from_path(out) : DSYM_FORMAT_YAML;
    if (fmt == DSYM_FORMAT_AUTO) fmt = DSYM_FORMAT_YAML;
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFD *db = archbfd_scan(dir, &d);
    if (!db) return finish(&d, 1);
    DsymBuf b;
    dsym_buf_init(&b);
    int rc = archbfd_write_catalogue(db, fmt, root, &b, &d) ? write_out(&b, out) : 1;
    dsym_buf_free(&b);
    archbfd_close(db);
    return finish(&d, rc);
}

static int cmd_info(int argc, char **argv) {
    DsymFormat in = DSYM_FORMAT_AUTO;
    const char *file = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-i") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &in)) return 2;
        } else
            file = argv[i];
    }
    if (!file) {
        usage(stderr);
        return 2;
    }
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFDSpec *s = archbfd_spec_load_file(file, in, 0, &d);
    if (!s) return finish(&d, 1);
    printf("format %s: %s\n", s->name, s->description ? s->description : "");
    printf("  object format: %s\n", s->has_format ? archbfd_format_name(s->format) : "-");
    printf("  byte order:    %s\n",
           s->has_byte_order ? archbfd_byte_order_name(s->byte_order) : "-");
    printf("  target vector: %s\n", s->target_vector ? s->target_vector : "-");
    printf("  architecture:  %s\n", s->arch ? s->arch : "-");
    printf("  container:     %s\n", s->container ? s->container : "-");
    if (s->magic) printf("  magic:         %s\n", s->magic);
    printf("  records: %zu, constants: %zu groups, relocations: %zu, sections: %zu\n",
           s->record_count, s->constant_group_count, s->reloc_count, s->section_count);
    for (size_t i = 0; i < s->source_count; i++)
        printf("  source: %s%s%s\n", s->sources[i].file, s->sources[i].note ? " — " : "",
               s->sources[i].note ? s->sources[i].note : "");
    archbfd_spec_free(s);
    return finish(&d, 0);
}

static int cmd_relocs(int argc, char **argv) {
    DsymFormat in = DSYM_FORMAT_AUTO;
    const char *file = NULL, *want = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-i") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &in)) return 2;
        } else if (!file)
            file = argv[i];
        else
            want = argv[i];
    }
    if (!file) {
        usage(stderr);
        return 2;
    }
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFDSpec *s = archbfd_spec_load_file(file, in, 0, &d);
    if (!s) return finish(&d, 1);
    int rc = 0;
    if (want) {
        int32_t idx = archbfd_find_reloc(s, want);
        if (idx < 0) {
            fprintf(stderr, "archbfd: no relocation '%s' in %s\n", want, file);
            rc = 1;
        } else {
            const ArchBFDReloc *r = &s->relocations[idx];
            printf("%s: number %" PRId64 ", %" PRId64 " bytes, %" PRId64 " bits%s\n", r->name,
                   r->number, r->size, r->bitsize, r->pcrel ? ", pc-relative" : "");
        }
    } else {
        printf("%-28s %6s %5s %6s %-10s\n", "NAME", "NUMBER", "SIZE", "BITS", "OVERFLOW");
        for (size_t i = 0; i < s->reloc_count; i++) {
            const ArchBFDReloc *r = &s->relocations[i];
            printf("%-28s %6" PRId64 " %5" PRId64 " %6" PRId64 " %-10s%s\n", r->name, r->number,
                   r->size, r->bitsize, archbfd_overflow_name(r->overflow),
                   r->pcrel ? " pc-relative" : "");
        }
    }
    archbfd_spec_free(s);
    return finish(&d, rc);
}

static int cmd_records(int argc, char **argv) {
    DsymFormat in = DSYM_FORMAT_AUTO;
    const char *file = NULL, *want = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-i") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &in)) return 2;
        } else if (!file)
            file = argv[i];
        else
            want = argv[i];
    }
    if (!file) {
        usage(stderr);
        return 2;
    }
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFDSpec *s = archbfd_spec_load_file(file, in, 0, &d);
    if (!s) return finish(&d, 1);
    int rc = 0;
    if (want) {
        const ArchBFDRecord *r = archbfd_find_record(s, want);
        if (!r) {
            fprintf(stderr, "archbfd: no record '%s' in %s\n", want, file);
            rc = 1;
        } else {
            printf("%s: %" PRId64 " bytes\n", r->name, r->size);
            for (size_t i = 0; i < r->field_count; i++)
                printf("  %-16s offset %3" PRId64 " size %3" PRId64 " %s\n", r->fields[i].name,
                       r->fields[i].offset, r->fields[i].size,
                       r->fields[i].desc ? r->fields[i].desc : "");
        }
    } else {
        for (size_t i = 0; i < s->record_count; i++)
            printf("%-16s %4" PRId64 " bytes, %zu fields\n", s->records[i].name, s->records[i].size,
                   s->records[i].field_count);
    }
    archbfd_spec_free(s);
    return finish(&d, rc);
}

static int cmd_object(int argc, char **argv) {
    const char *file = NULL, *format = NULL;
    const char *catalogue = "ArchBFDCatalogue.yaml";
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-f") && i + 1 < argc)
            format = argv[++i];
        else if (!strcmp(argv[i], "--catalogue") && i + 1 < argc)
            catalogue = argv[++i];
        else if (!file)
            file = argv[i];
        else {
            usage(stderr);
            return 2;
        }
    }
    if (!file) {
        usage(stderr);
        return 2;
    }
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFD *db = archbfd_open(catalogue, &d);
    if (!db) return finish(&d, 1);
    /* Pick the description: an explicit name, or the first one whose magic
       matches the object and that can read it. */
    const ArchBFDSpec *spec = NULL;
    if (format) {
        spec = archbfd_load_name(db, format, 0, &d);
    } else {
        DsymBuf head;
        dsym_buf_init(&head);
        if (dsym_buf_read_file(&head, file)) {
            for (size_t i = 0; i < archbfd_count(db) && !spec; i++) {
                const ArchBFDEntry *e = archbfd_entry(db, i);
                const ArchBFDSpec *s = archbfd_load(db, e, 0, &d);
                if (!s || !s->magic || !s->magic_len) continue;
                uint8_t magic[16];
                size_t n = archbfd_magic_bytes(s, magic, sizeof magic);
                if (!n || head.len < n || memcmp(head.data, magic, n)) continue;
                /* Several descriptions may share the magic; keep the first
                   one that can read the object. */
                DsymDiags probe;
                dsym_diags_init(&probe);
                ArchBFDObject *o = archbfd_object_open_memory(head.data, head.len, s, &probe);
                dsym_diags_free(&probe);
                if (o) {
                    archbfd_object_close(o);
                    spec = s;
                }
            }
        }
        dsym_buf_free(&head);
    }
    if (!spec) {
        fprintf(stderr, "archbfd: no description matches '%s'\n", file);
        archbfd_close(db);
        return finish(&d, 1);
    }
    ArchBFDObject *obj = archbfd_object_open(file, spec, &d);
    if (!obj) {
        archbfd_close(db);
        return finish(&d, 1);
    }
    printf("%s: %s, machine %" PRId64 "\n", file, archbfd_object_kind(obj),
           archbfd_object_machine(obj));
    printf("sections (%zu):\n", archbfd_object_section_count(obj));
    for (size_t i = 0; i < archbfd_object_section_count(obj); i++) {
        const ArchBFDObjectSection *s = archbfd_object_section(obj, i);
        printf("  %-20s type %3" PRId64 " flags 0x%" PRIx64 " size %8" PRId64 " addr %8" PRId64
               "\n",
               s->name, s->type, s->flags, s->size, s->addr);
    }
    printf("symbols (%zu):\n", archbfd_object_symbol_count(obj));
    for (size_t i = 0; i < archbfd_object_symbol_count(obj); i++) {
        const ArchBFDObjectSymbol *s = archbfd_object_symbol(obj, i);
        printf("  %-24s value %8" PRId64 " size %6" PRId64 " %s%s\n", s->name, s->value, s->size,
               s->bind ? s->bind : "", s->type ? s->type : "");
    }
    printf("relocations (%zu):\n", archbfd_object_reloc_count(obj));
    for (size_t i = 0; i < archbfd_object_reloc_count(obj); i++) {
        const ArchBFDObjectReloc *r = archbfd_object_reloc(obj, i);
        printf("  %-24s offset %6" PRId64 " symbol %s\n",
               r->type_name ? r->type_name : "?", r->offset,
               r->symbol_name ? r->symbol_name : "-");
    }
    archbfd_object_close(obj);
    archbfd_close(db);
    return finish(&d, 0);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    const char *cmd = argv[1];
    argc -= 2;
    argv += 2;
    if (!strcmp(cmd, "validate")) return cmd_validate(argc, argv);
    if (!strcmp(cmd, "convert")) return cmd_convert(argc, argv);
    if (!strcmp(cmd, "list")) return cmd_list(argc, argv);
    if (!strcmp(cmd, "catalogue") || !strcmp(cmd, "catalog")) return cmd_catalogue(argc, argv);
    if (!strcmp(cmd, "info")) return cmd_info(argc, argv);
    if (!strcmp(cmd, "relocs") || !strcmp(cmd, "relocations")) return cmd_relocs(argc, argv);
    if (!strcmp(cmd, "records")) return cmd_records(argc, argv);
    if (!strcmp(cmd, "object")) return cmd_object(argc, argv);
    if (!strcmp(cmd, "-h") || !strcmp(cmd, "--help") || !strcmp(cmd, "help")) {
        usage(stdout);
        return 0;
    }
    fprintf(stderr, "archbfd: unknown command '%s'\n", cmd);
    usage(stderr);
    return 2;
}
