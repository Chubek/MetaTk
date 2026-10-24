/*
 * archdb — command-line front end for the MetaArch architecture database.
 *
 *   archdb validate [--strict] [--lint] [--backend] FILE...
 *   archdb convert  -f FORMAT [-o OUT] [--embed-backend|--ref-backend]
 *                   [--compact] [--no-header] FILE
 *   archdb list     [CATALOGUE|DIR]
 *   archdb catalogue DIR [-o OUT] [--root ROOT]
 *   archdb encode   FILE ENCODER [name=value ...] [--target N]
 *   archdb dump-ir  FILE [ENCODER]
 *   archdb info     FILE
 *
 * FORMAT is yaml, json, xml or sexpr. Input format follows the extension
 * (override with -i FORMAT). Exit status: 0 ok, 1 invalid input, 2 usage.
 */
#include "ArchDB.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *f) {
    fputs("usage:\n"
          "  archdb validate [--strict] [--lint] [--backend] [-i FMT] FILE...\n"
          "  archdb convert -f FMT [-o OUT] [-i FMT] [--embed-backend|--ref-backend]\n"
          "                 [--no-backend-load] [--compact] [--no-header] FILE\n"
          "  archdb list [CATALOGUE|DIR]\n"
          "  archdb catalogue DIR [-o OUT] [-f FMT] [--root ROOT]\n"
          "  archdb encode [-i FMT] FILE ENCODER [name=value ...] [--target N]\n"
          "  archdb dump-ir [-i FMT] FILE [ENCODER]\n"
          "  archdb info [-i FMT] FILE\n"
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
        fprintf(stderr, "archdb: unknown format '%s'\n", s ? s : "");
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
        if (!strcmp(argv[i], "--strict")) flags |= DSYM_LOAD_STRICT;
        else if (!strcmp(argv[i], "--lint")) flags |= DSYM_LOAD_LINT;
        else if (!strcmp(argv[i], "--backend")) flags |= DSYM_LOAD_BACKEND;
        else if (!strcmp(argv[i], "-i") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &in)) return 2;
        } else {
            DsymDiags d;
            dsym_diags_init(&d);
            DsymSpec *s = dsym_spec_load_file(argv[i], in, flags, &d);
            files++;
            if (!s || ((flags & DSYM_LOAD_STRICT) && d.warnings)) bad++;
            else
                printf("%s: ok (%zu registers, %zu encoders, %zu forms)\n", argv[i],
                       s->register_count, s->encoder_count, s->form_count);
            dsym_spec_free(s);
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
    uint32_t wflags = 0, lflags = DSYM_LOAD_BACKEND;
    const char *out = NULL, *file = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-f") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &outf)) return 2;
        } else if (!strcmp(argv[i], "-i") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &in)) return 2;
        } else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--embed-backend")) wflags |= DSYM_WRITE_EMBED_BACKEND;
        else if (!strcmp(argv[i], "--ref-backend")) wflags |= DSYM_WRITE_REF_BACKEND;
        else if (!strcmp(argv[i], "--no-backend-load")) lflags &= ~DSYM_LOAD_BACKEND;
        else if (!strcmp(argv[i], "--compact")) wflags |= DSYM_WRITE_COMPACT;
        else if (!strcmp(argv[i], "--no-header")) wflags |= DSYM_WRITE_NO_HEADER;
        else if (!file) file = argv[i];
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
    /* Only load the backend when it will be embedded. */
    if (!(wflags & DSYM_WRITE_EMBED_BACKEND) &&
        (outf != DSYM_FORMAT_JSON || (wflags & DSYM_WRITE_REF_BACKEND)))
        lflags &= ~DSYM_LOAD_BACKEND;
    DsymDiags d;
    dsym_diags_init(&d);
    DsymSpec *s = dsym_spec_load_file(file, in, lflags, &d);
    if (!s) return finish(&d, 1);
    DsymBuf b;
    dsym_buf_init(&b);
    int rc = dsym_spec_write(s, outf, wflags, &b, &d) ? write_out(&b, out) : 1;
    dsym_buf_free(&b);
    dsym_spec_free(s);
    return finish(&d, rc);
}

static int cmd_list(int argc, char **argv) {
    const char *path = argc > 0 ? argv[0] : "ArchDBCatalogue.yaml";
    DsymDiags d;
    dsym_diags_init(&d);
    ArchDB *db = archdb_open(path, &d);
    if (!db) return finish(&d, 1);
    printf("%-12s %-10s %-12s %5s %-7s %-8s %s\n", "NAME", "ARCH", "SUBARCH", "BITS", "ORDER",
           "FORMAT", "FILE");
    for (size_t i = 0; i < archdb_count(db); i++) {
        const ArchDBEntry *e = archdb_entry(db, i);
        printf("%-12s %-10s %-12s %5" PRId64 " %-7s %-8s %s\n", e->name, e->arch ? e->arch : "-",
               e->subarch ? e->subarch : "-", e->addr_size, e->byte_order ? e->byte_order : "-",
               e->obj_format ? e->obj_format : "-", e->path);
    }
    archdb_close(db);
    return finish(&d, 0);
}

static int cmd_catalogue(int argc, char **argv) {
    const char *dir = NULL, *out = NULL, *root = NULL;
    DsymFormat fmt = DSYM_FORMAT_AUTO;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--root") && i + 1 < argc) root = argv[++i];
        else if (!strcmp(argv[i], "-f") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &fmt)) return 2;
        } else dir = argv[i];
    }
    if (!dir) {
        usage(stderr);
        return 2;
    }
    if (fmt == DSYM_FORMAT_AUTO) fmt = out ? dsym_format_from_path(out) : DSYM_FORMAT_YAML;
    if (fmt == DSYM_FORMAT_AUTO) fmt = DSYM_FORMAT_YAML;
    DsymDiags d;
    dsym_diags_init(&d);
    ArchDB *db = archdb_scan(dir, &d);
    if (!db) return finish(&d, 1);
    DsymBuf b;
    dsym_buf_init(&b);
    int rc = archdb_write_catalogue(db, fmt, root, &b, &d) ? write_out(&b, out) : 1;
    dsym_buf_free(&b);
    archdb_close(db);
    return finish(&d, rc);
}

static int cmd_encode(int argc, char **argv) {
    DsymFormat in = DSYM_FORMAT_AUTO;
    const char *file = NULL, *enc = NULL;
    const char *names[64];
    int64_t vals[64];
    size_t n = 0, target = 0;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-i") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &in)) return 2;
        } else if (!strcmp(argv[i], "--target") && i + 1 < argc) target = strtoul(argv[++i], NULL, 0);
        else if (!file) file = argv[i];
        else if (!enc) enc = argv[i];
        else {
            char *eq = strchr(argv[i], '=');
            if (!eq || n == 64) {
                fprintf(stderr, "archdb: expected name=value, got '%s'\n", argv[i]);
                return 2;
            }
            *eq = 0;
            char *end;
            long long v = strtoll(eq + 1, &end, 0);
            if (*end) { /* allow unsigned 64-bit hex */
                v = (long long)strtoull(eq + 1, &end, 0);
                if (*end) {
                    fprintf(stderr, "archdb: bad number '%s'\n", eq + 1);
                    return 2;
                }
            }
            names[n] = argv[i];
            vals[n++] = v;
        }
    }
    if (!file || !enc) {
        usage(stderr);
        return 2;
    }
    DsymDiags d;
    dsym_diags_init(&d);
    DsymSpec *s = dsym_spec_load_file(file, in, 0, &d);
    if (!s) return finish(&d, 1);
    DsymEncoded out;
    int rc = 0;
    if (!dsym_encode_named(s, enc, target, names, vals, n, false, &out)) {
        fprintf(stderr, "archdb: %s\n", out.error);
        int32_t ei = dsym_spec_find_encoder(s, enc);
        if (ei >= 0 && s->encoders[ei].program) {
            fprintf(stderr, "operands:");
            for (size_t k = 0; k < s->encoders[ei].program->param_count; k++)
                fprintf(stderr, " %s", s->encoders[ei].program->params[k]);
            fputc('\n', stderr);
        }
        rc = 1;
    } else {
        for (size_t k = 0; k < out.size; k++) printf("%s%02x", k ? " " : "", out.bytes[k]);
        putchar('\n');
        for (size_t k = 0; k < out.reloc_count; k++)
            printf("reloc %s at +%u (%u bytes) -> %s=%" PRId64 "\n", out.relocs[k].reloc_name,
                   out.relocs[k].offset, out.relocs[k].size,
                   s->encoders[dsym_spec_find_encoder(s, enc)].program->params[out.relocs[k].sym_param],
                   out.relocs[k].sym_value);
    }
    dsym_spec_free(s);
    return finish(&d, rc);
}

static int cmd_dump_ir(int argc, char **argv) {
    DsymFormat in = DSYM_FORMAT_AUTO;
    const char *file = NULL, *enc = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-i") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &in)) return 2;
        } else if (!file) file = argv[i];
        else enc = argv[i];
    }
    if (!file) {
        usage(stderr);
        return 2;
    }
    DsymDiags d;
    dsym_diags_init(&d);
    DsymSpec *s = dsym_spec_load_file(file, in, 0, &d);
    if (!s) return finish(&d, 1);
    DsymBuf b;
    dsym_buf_init(&b);
    int rc = 0;
    for (size_t i = 0; i < s->encoder_count; i++) {
        if (enc && strcmp(enc, s->encoders[i].name)) continue;
        dsym_buf_printf(&b, "encoder %s\n", s->encoders[i].name);
        dsym_program_dump(s->encoders[i].program, &b);
    }
    if (enc && !b.len) {
        fprintf(stderr, "archdb: no encoder '%s'\n", enc);
        rc = 1;
    }
    write_out(&b, NULL);
    dsym_buf_free(&b);
    dsym_spec_free(s);
    return finish(&d, rc);
}

static int cmd_info(int argc, char **argv) {
    DsymFormat in = DSYM_FORMAT_AUTO;
    const char *file = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-i") && i + 1 < argc) {
            if (!parse_fmt(argv[++i], &in)) return 2;
        } else file = argv[i];
    }
    if (!file) {
        usage(stderr);
        return 2;
    }
    DsymDiags d;
    dsym_diags_init(&d);
    DsymSpec *s = dsym_spec_load_file(file, in, 0, &d);
    if (!s) return finish(&d, 1);
    for (size_t i = 0; i < s->target_count; i++) {
        const DsymTarget *t = &s->targets[i];
        printf("target %s%s%s: %" PRId64 "-bit %s-endian %s %s %s\n", t->arch,
               t->subarch ? "/" : "", t->subarch ? t->subarch : "", t->addr_size,
               dsym_target_is_big_endian(t) ? "big" : "little", t->obj_format ? t->obj_format : "",
               t->abi ? t->abi : "", t->calling_conv ? t->calling_conv : "");
    }
    printf("features %zu, registers %zu, mnemonics %zu, encoders %zu, forms %zu\n",
           s->feature_count, s->register_count, s->mnemonic_count, s->encoder_count, s->form_count);
    printf("syntaxes %zu, sections %zu, linkages %zu, relocs %zu, debug %zu\n", s->syntax_count,
           s->section_count, s->linkage_count, s->reloc_count, s->debug_count);
    if (s->asmtk)
        printf("asmtk: %zu encodings, %zu conditions\n", s->asmtk->encoding_count,
               s->asmtk->condition_count);
    if (s->metachain)
        printf("metachain: %s / %s, %zu values\n", s->metachain->scheduling_model,
               s->metachain->register_class, s->metachain->value_count);
    if (s->backend) printf("backend: %s\n", s->backend->file ? s->backend->file : "(embedded)");
    dsym_spec_free(s);
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
    if (!strcmp(cmd, "encode")) return cmd_encode(argc, argv);
    if (!strcmp(cmd, "dump-ir")) return cmd_dump_ir(argc, argv);
    if (!strcmp(cmd, "info")) return cmd_info(argc, argv);
    if (!strcmp(cmd, "-h") || !strcmp(cmd, "--help") || !strcmp(cmd, "help")) {
        usage(stdout);
        return 0;
    }
    fprintf(stderr, "archdb: unknown command '%s'\n", cmd);
    usage(stderr);
    return 2;
}
