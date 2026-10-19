/*
 * test_archdb — regression tests for MetaArch.
 *
 *   test_archdb ARCHDB_DIR CATALOGUE
 *
 * 1. Every spec loads cleanly (with its backend) and survives a round trip
 *    through YAML, JSON, XML and S-expressions with identical content.
 * 2. Generic trees with awkward values (controls, Unicode, huge integers,
 *    reals, empty containers, odd keys) round-trip through every codec.
 * 3. Encoders produce known machine code for several architectures.
 * 4. Invalid documents are rejected with located diagnostics.
 * 5. The catalogue opens and resolves names.
 */
#include "ArchDB.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
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

/* ---- 1. spec round trips ------------------------------------------------ */

static bool canonical(const DsymSpec *s, DsymBuf *out) {
    DsymDiags d;
    dsym_diags_init(&d);
    bool ok = dsym_spec_write(s, DSYM_FORMAT_JSON, DSYM_WRITE_COMPACT | DSYM_WRITE_EMBED_BACKEND, out, &d);
    dsym_diags_free(&d);
    return ok;
}

static void roundtrip_spec(const char *path) {
    DsymDiags d;
    dsym_diags_init(&d);
    DsymSpec *s = dsym_spec_load_file(path, DSYM_FORMAT_AUTO, DSYM_LOAD_BACKEND, &d);
    CHECK(s != NULL, "load %s", path);
    CHECK(d.errors == 0 && d.warnings == 0, "%s: %zu error(s), %zu warning(s)", path, d.errors,
          d.warnings);
    if (d.count) dsym_diags_print(&d, stderr);
    dsym_diags_free(&d);
    if (!s) return;
    CHECK(!s->backend || s->backend->document, "%s: backend not loaded", path);

    DsymBuf ref;
    dsym_buf_init(&ref);
    CHECK(canonical(s, &ref), "%s: canonical JSON", path);

    for (size_t f = 0; f < sizeof FORMATS / sizeof *FORMATS; f++) {
        DsymBuf text, again;
        dsym_buf_init(&text);
        dsym_buf_init(&again);
        dsym_diags_init(&d);
        bool ok = dsym_spec_write(s, FORMATS[f], DSYM_WRITE_EMBED_BACKEND, &text, &d);
        CHECK(ok, "%s: write %s", path, dsym_format_name(FORMATS[f]));
        char vpath[1200];
        snprintf(vpath, sizeof vpath, "%s.roundtrip%s", path, dsym_format_extension(FORMATS[f]));
        DsymSpec *s2 = ok ? dsym_spec_load_buffer(text.data, text.len, FORMATS[f], vpath, 0, &d)
                          : NULL;
        CHECK(s2 != NULL, "%s: re-read %s", path, dsym_format_name(FORMATS[f]));
        if (!s2) dsym_diags_print(&d, stderr);
        if (s2) {
            CHECK(canonical(s2, &again), "%s: canonical (2)", path);
            CHECK(again.len == ref.len && memcmp(again.data, ref.data, ref.len) == 0,
                  "%s: %s round trip differs (%zu vs %zu bytes)", path,
                  dsym_format_name(FORMATS[f]), again.len, ref.len);
            CHECK(s2->encoder_count == s->encoder_count && s2->form_count == s->form_count,
                  "%s: record counts", path);
        }
        dsym_spec_free(s2);
        dsym_diags_free(&d);
        dsym_buf_free(&text);
        dsym_buf_free(&again);
    }

    /* reference-style YAML keeps `backend: {file: ...}` */
    DsymBuf y;
    dsym_buf_init(&y);
    dsym_spec_write(s, DSYM_FORMAT_YAML, 0, &y, NULL);
    if (s->backend && s->backend->file)
        CHECK(y.data && strstr(y.data, "backend: {file: backend/"), "%s: backend reference", path);
    dsym_buf_free(&y);
    dsym_buf_free(&ref);
    dsym_spec_free(s);
}

static int cmp(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static size_t roundtrip_dir(const char *dir) {
    DIR *dp = opendir(dir);
    CHECK(dp != NULL, "opendir %s", dir);
    if (!dp) return 0;
    char *names[256];
    size_t n = 0;
    struct dirent *de;
    while ((de = readdir(dp)) && n < 256)
        if (strstr(de->d_name, ".dsymspec.yaml")) names[n++] = strdup(de->d_name);
    closedir(dp);
    qsort(names, n, sizeof *names, cmp);
    for (size_t i = 0; i < n; i++) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        roundtrip_spec(path);
        free(names[i]);
    }
    return n;
}

/* ---- 2. generic tree round trips --------------------------------------- */

static DsymNode *awkward_tree(DsymArena *a, bool json_safe) {
    DsymNode *m = dsym_node_map(a);
    dsym_map_put_ref(a, m, "plain", dsym_node_str(a, "hello"));
    dsym_map_put_ref(a, m, "looks_like_int", dsym_node_str(a, "0x90"));
    dsym_map_put_ref(a, m, "looks_like_bool", dsym_node_str(a, "true"));
    dsym_map_put_ref(a, m, "yes", dsym_node_str(a, "yes"));
    dsym_map_put_ref(a, m, "empty", dsym_node_str(a, ""));
    dsym_map_put_ref(a, m, "space", dsym_node_str(a, "  leading and trailing  "));
    dsym_map_put_ref(a, m, "multi", dsym_node_str(a, "EMIT8 0x90\nEMIT8 0xC3\n"));
    dsym_map_put_ref(a, m, "controls", dsym_node_strn(a, "a\x01\x1f\r\tb\"\\|;#", 12));
    dsym_map_put_ref(a, m, "nul", dsym_node_strn(a, "x\0y", 3));
    dsym_map_put_ref(a, m, "unicode", dsym_node_str(a, "\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80 caf\xc3\xa9"));
    dsym_map_put_ref(a, m, "symbols", dsym_node_str(a, "$r0 %f0 16-bit {a: [b]} - * & ! ' \""));
    dsym_map_put_ref(a, m, "int_min", dsym_node_int(a, INT64_MIN));
    dsym_map_put_ref(a, m, "int_max", dsym_node_int(a, INT64_MAX));
    dsym_map_put_ref(a, m, "uint_max", dsym_node_uint(a, UINT64_MAX));
    dsym_map_put_ref(a, m, "real", dsym_node_real(a, 0.1));
    dsym_map_put_ref(a, m, "real_int", dsym_node_real(a, 3.0));
    dsym_map_put_ref(a, m, "tiny", dsym_node_real(a, 5e-324));
    if (!json_safe) {
        dsym_map_put_ref(a, m, "inf", dsym_node_real(a, -INFINITY));
    }
    dsym_map_put_ref(a, m, "null", dsym_node_null(a));
    dsym_map_put_ref(a, m, "t", dsym_node_bool(a, true));
    dsym_map_put_ref(a, m, "f", dsym_node_bool(a, false));
    dsym_map_put_ref(a, m, "empty_seq", dsym_node_seq(a));
    dsym_map_put_ref(a, m, "empty_map", dsym_node_map(a));
    dsym_map_put_ref(a, m, "key with spaces|and:colons", dsym_node_int(a, 1));
    dsym_map_put_ref(a, m, "123", dsym_node_int(a, 2));
    dsym_map_put_ref(a, m, "", dsym_node_str(a, "empty key"));
    DsymNode *nested = dsym_node_seq(a);
    for (int i = 0; i < 3; i++) {
        DsymNode *inner = dsym_node_map(a);
        dsym_map_put_ref(a, inner, "i", dsym_node_int(a, i));
        DsymNode *deep = dsym_node_seq(a);
        dsym_seq_push(a, deep, dsym_node_seq(a));
        dsym_seq_push(a, deep, dsym_node_map(a));
        dsym_seq_push(a, deep, dsym_node_str(a, "-"));
        dsym_map_put_ref(a, inner, "deep", deep);
        dsym_seq_push(a, nested, inner);
    }
    dsym_map_put_ref(a, m, "nested", nested);
    return m;
}

static void roundtrip_trees(void) {
    for (size_t f = 0; f < sizeof FORMATS / sizeof *FORMATS; f++) {
        for (int compact = 0; compact < 2; compact++) {
            DsymArena *a = dsym_arena_new();
            DsymNode *t = awkward_tree(a, FORMATS[f] == DSYM_FORMAT_JSON);
            DsymBuf b;
            dsym_buf_init(&b);
            DsymDiags d;
            dsym_diags_init(&d);
            bool ok = dsym_tree_write(FORMATS[f], t, &b, compact ? DSYM_WRITE_COMPACT : 0, &d);
            CHECK(ok, "tree write %s", dsym_format_name(FORMATS[f]));
            DsymNode *back = ok ? dsym_tree_read(FORMATS[f], a, b.data, b.len, "mem", &d) : NULL;
            CHECK(back != NULL, "tree read %s", dsym_format_name(FORMATS[f]));
            if (!back) {
                dsym_diags_print(&d, stderr);
                fprintf(stderr, "%s\n", b.data);
            }
            bool eq = back && dsym_node_equal(t, back);
            CHECK(eq, "tree %s (compact=%d) round trip differs", dsym_format_name(FORMATS[f]),
                  compact);
            if (back && !eq) fprintf(stderr, "%s\n", b.data);
            dsym_diags_free(&d);
            dsym_buf_free(&b);
            dsym_arena_free(a);
        }
    }
}

/* ---- 3. encodings ------------------------------------------------------ */

static void expect_bytes(DsymSpec *s, const char *arch, const char *enc,
                         const char *const *names, const int64_t *vals, size_t n,
                         const char *hex) {
    DsymEncoded out;
    bool ok = dsym_encode_named(s, enc, 0, names, vals, n, false, &out);
    CHECK(ok, "%s %s: %s", arch, enc, out.error);
    if (!ok) return;
    char got[256] = "";
    for (size_t i = 0; i < out.size; i++)
        snprintf(got + strlen(got), sizeof got - strlen(got), "%s%02x", i ? " " : "", out.bytes[i]);
    CHECK(strcmp(got, hex) == 0, "%s %s: got [%s], want [%s]", arch, enc, got, hex);
}

static DsymSpec *load(const char *dir, const char *arch) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.dsymspec.yaml", dir, arch);
    DsymDiags d;
    dsym_diags_init(&d);
    DsymSpec *s = dsym_spec_load_file(path, DSYM_FORMAT_AUTO, 0, &d);
    if (!s) dsym_diags_print(&d, stderr);
    dsym_diags_free(&d);
    CHECK(s != NULL, "load %s", path);
    return s;
}

static void encoding_table(const char *dir) {
    FILE *f = fopen(ENCODINGS_TSV, "r");
    CHECK(f != NULL, "open %s", ENCODINGS_TSV);
    if (!f) return;
    char line[2048];
    size_t lineno = 0, rows = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        if (line[0] == '#' || line[0] == '\n') continue;
        char *cols[4], *next = line;
        bool valid = true;
        for (size_t i = 0; i < 4; i++) {
            cols[i] = next;
            char *sep = strchr(next, i < 3 ? '\t' : '\n');
            if (!sep && i < 3) { valid = false; break; }
            if (sep) { *sep = 0; next = sep + 1; }
        }
        CHECK(valid, "TSV row %zu: expected four tab-separated columns", lineno);
        if (!valid) continue;
        cols[3][strcspn(cols[3], "\r")] = 0;
        const char *names[32];
        int64_t values[32];
        size_t n = 0;
        char *save = NULL;
        for (char *arg = strtok_r(cols[2], " ", &save); arg;
             arg = strtok_r(NULL, " ", &save)) {
            char *eq = strchr(arg, '=');
            if (!eq || eq == arg || n == 32) { valid = false; break; }
            *eq++ = 0;
            char *end;
            errno = 0;
            intmax_t value = strtoimax(eq, &end, 0);
            if (!*eq || *end || errno || value < INT64_MIN || value > INT64_MAX) {
                valid = false;
                break;
            }
            names[n] = arg;
            values[n++] = (int64_t)value;
        }
        CHECK(valid, "TSV row %zu: invalid operand assignment", lineno);
        if (!valid) continue;
        DsymSpec *s = load(dir, cols[0]);
        if (s) {
            expect_bytes(s, cols[0], cols[1], names, values, n, cols[3]);
            dsym_spec_free(s);
        }
        rows++;
    }
    CHECK(!ferror(f) && rows > 0, "read encoding table");
    fclose(f);
}

static void encodings(const char *dir) {
    DsymSpec *s;
    if ((s = load(dir, "x86"))) {
        const char *n[] = {"dst", "src"};
        int64_t v[] = {0 /* eax */, 1 /* ecx */};
        expect_bytes(s, "x86", "add", n, v, 2, "01 c8"); /* add eax, ecx */
        expect_bytes(s, "x86", "ret", NULL, NULL, 0, "c3");
        const char *ni[] = {"dst", "imm"};
        int64_t vi[] = {0, 0x12345678};
        expect_bytes(s, "x86", "movimm", ni, vi, 2, "b8 78 56 34 12 00 00 00 00");
        /* call: E8 + rel32 placeholder + relocation at offset 1 */
        DsymEncoded out;
        const char *ns[] = {"sym"};
        int64_t vs[] = {42};
        CHECK(dsym_encode_named(s, "call", 0, ns, vs, 1, false, &out), "x86 call");
        CHECK(out.size == 5 && out.reloc_count == 1 && out.relocs[0].offset == 1 &&
                  out.relocs[0].size == 4 && !strcmp(out.relocs[0].reloc_name, "R_386_PLT32") &&
                  out.relocs[0].reloc_index == dsym_spec_find_reloc(s, "R_386_PLT32") &&
                  out.relocs[0].sym_value == 42,
              "x86 call relocation");
        CHECK(out.relocs[0].bit_offset == 0 && out.relocs[0].bit_width == 32 &&
              out.relocs[0].endian == DSYM_ENDIAN_LITTLE, "x86 call relocation bit metadata");
        /* unbound operand is an error */
        CHECK(!dsym_encode_named(s, "add", 0, n, v, 1, false, &out), "unbound operand");
        /* lookups */
        int32_t eax = dsym_spec_find_register(s, "eax");
        CHECK(eax >= 0 && s->registers[eax].parent_index == dsym_spec_find_register(s, "rax"),
              "eax parent");
        int32_t mi = dsym_spec_find_mnemonic(s, "add");
        int32_t forms[8];
        size_t nf = dsym_spec_forms_for_mnemonic(s, mi, forms, 8);
        CHECK(nf >= 1 && !strcmp(s->forms[forms[0]].mnemonic, "add"), "forms for add");
        dsym_spec_free(s);
    }
    if ((s = load(dir, "riscv64"))) {
        const char *n[] = {"rd", "rs1", "imm"};
        int64_t v[] = {10, 10, 1};
        expect_bytes(s, "riscv64", "addi", n, v, 3, "13 05 15 00"); /* addi a0,a0,1 */
        int64_t v2[] = {10, 10, -1};
        expect_bytes(s, "riscv64", "addi", n, v2, 3, "13 05 f5 ff"); /* addi a0,a0,-1 */
        int32_t form = -1;
        int32_t mnemonic = dsym_spec_find_mnemonic(s, "addi");
        CHECK(dsym_spec_forms_for_mnemonic(s, mnemonic, &form, 1) > 0 && form >= 0,
              "find RISC-V addi form");
        if (form >= 0) {
            DsymEncoded out;
            CHECK(dsym_encode_form(s, (size_t)form, 0, v2, 3, &out) && out.size == 4 &&
                  !memcmp(out.bytes, "\x13\x05\xf5\xff", 4), "checked RISC-V addi anchor");
            int64_t bad[] = {10, 10, -2049};
            CHECK(!dsym_encode_form(s, (size_t)form, 0, bad, 3, &out) &&
                  strstr(out.error, "operand 3 (imm)"), "checked immediate cannot wrap below range");
            bad[2] = 2048;
            CHECK(!dsym_encode_form(s, (size_t)form, 0, bad, 3, &out),
                  "checked immediate cannot wrap above range");
            CHECK(dsym_encode(s, dsym_spec_find_encoder(s, "addi"), 0, bad, 3, &out),
                  "low-level modulo behavior unchanged");
            bad[0] = 32;
            bad[2] = 0;
            CHECK(!dsym_encode_form(s, (size_t)form, 0, bad, 3, &out) &&
                  strstr(out.error, "operand 1 (reg)"), "checked RISC-V register encoding");
        }
        expect_bytes(s, "riscv64", "ret", NULL, NULL, 0, "67 80 00 00");
        dsym_spec_free(s);
    }
    if ((s = load(dir, "aarch64"))) {
        const char *n[] = {"rd", "rn", "rm"};
        int64_t v[] = {0, 1, 2};
        expect_bytes(s, "aarch64", "add", n, v, 3, "20 00 02 8b"); /* add x0,x1,x2 */
        expect_bytes(s, "aarch64", "ret", NULL, NULL, 0, "c0 03 5f d6");
        dsym_spec_free(s);
    }
    if ((s = load(dir, "m68k"))) {
        expect_bytes(s, "m68k", "nop", NULL, NULL, 0, "4e 71");
        expect_bytes(s, "m68k", "rts", NULL, NULL, 0, "4e 75");
        dsym_spec_free(s);
    }
}

/* ---- 4. diagnostics ----------------------------------------------------- */

static void expect_error(const char *doc, const char *needle, uint32_t line) {
    DsymDiags d;
    dsym_diags_init(&d);
    DsymSpec *s = dsym_spec_load_buffer(doc, strlen(doc), DSYM_FORMAT_YAML, "bad.yaml", 0, &d);
    CHECK(s == NULL, "accepted invalid document expecting '%s'", needle);
    bool found = false;
    for (size_t i = 0; i < d.count; i++)
        if (strstr(d.items[i].msg, needle) && (!line || d.items[i].line == line)) found = true;
    CHECK(found, "expected diagnostic '%s' (line %u)", needle, line);
    if (!found) dsym_diags_print(&d, stderr);
    dsym_spec_free(s);
    dsym_diags_free(&d);
}

#define HEAD "schema: 1\ntargets:\n  - {arch: t, addr_size: 32, byte_order: little}\n"

static void biased_lint_tests(void) {
    const struct { const char *body; bool biased; } cases[] = {
        {"EMIT32LE ((imm + 32768) % 65536)", true},
        {"EMIT32LE 17 + ((imm + (128 * 256)) % (256 * 256))*2", true},
        {"EMIT32LE ((imm + 65536) % 65536)", false},
        {"EMIT32LE ((imm + 0) % 65536)", false},
        {"EMIT32LE ((imm + 3) % 7)", false},
        {"EMIT32LE ((imm + 32768) % 65536) # biased", false},
        {"EMIT32LE ((imm + 32768) % 65536) # biasedness", true},
        {"EMIT32LE ((1 + 32768) % 65536)", false},
        {"EMIT_MODRM 0, ((imm + 4) % 8)", true},
        {"REQUIRE ((imm + 4) % 8) == 0", true},
        {"EMIT_IMM 4, ((imm + 4) % 8)", true},
    };
    const uint32_t flags[] = {0, DSYM_LOAD_LINT, DSYM_LOAD_STRICT,
                              DSYM_LOAD_LINT | DSYM_LOAD_STRICT};
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        for (size_t j = 0; j < sizeof flags / sizeof *flags; j++) {
            DsymBuf doc;
            dsym_buf_init(&doc);
            dsym_buf_printf(&doc, HEAD "encoders:\n  - name: test_bias\n    body: |\n      %s\n",
                            cases[i].body);
            DsymDiags d;
            dsym_diags_init(&d);
            DsymSpec *s = dsym_spec_load_buffer(doc.data, doc.len, DSYM_FORMAT_YAML,
                                               "bias.yaml", flags[j], &d);
            bool strict_error = cases[i].biased && (flags[j] & DSYM_LOAD_STRICT);
            CHECK((s != NULL) == !strict_error, "bias case %zu flags %u: load", i, flags[j]);
            size_t found = 0;
            for (size_t k = 0; k < d.count; k++) {
                if (!strstr(d.items[k].msg, "DSYM-W001")) continue;
                found++;
                CHECK(d.items[k].severity == (strict_error ? DSYM_ERROR : DSYM_WARNING) &&
                      strstr(d.items[k].msg, "test_bias") && strstr(d.items[k].msg, "statement 1"),
                      "bias diagnostic severity and context");
            }
            CHECK(found == (size_t)(cases[i].biased && flags[j] != 0),
                  "bias case %zu flags %u: diagnostic count", i, flags[j]);
            dsym_spec_free(s);
            dsym_diags_free(&d);
            dsym_buf_free(&doc);
        }
    }
}

static void diagnostics(void) {
    expect_error("schema: 2\ntargets: [{arch: t, addr_size: 8, byte_order: le}]\n", "must be 1", 1);
    expect_error("schema: 1\n", "missing required key 'targets'", 1);
    expect_error("schema: 1\ntargets: []\n", "at least 1", 2);
    expect_error(HEAD "bogus: 1\n", "unknown key 'bogus'", 4);
    expect_error("schema: 1\ntargets:\n  - {arch: t, addr_size: 32, byte_order: middle}\n",
                 "'middle' is not one of", 3);
    expect_error("schema: 1\ntargets:\n  - {arch: t, addr_size: 32, byte_order: le, align: 3}\n",
                 "not a power of two", 0);
    expect_error("schema: 1\ntargets:\n  - {arch: \"has space\", addr_size: 32, byte_order: le}\n",
                 "printable ASCII", 3);
    expect_error(HEAD "registers:\n  - {name: r0, class: gpr, width: 24, enc: 0}\n",
                 "24 is not one of", 5);
    expect_error(HEAD "registers:\n  - {name: r0, class: gpr, width: 32, enc: 0, parent: zz}\n",
                 "unknown parent register 'zz'", 0);
    expect_error(HEAD "features:\n  - {name: a}\n  - {name: a}\n", "duplicate feature name", 0);
    expect_error(HEAD "encoders:\n  - name: e\n    body: |\n      EMIT8 0x90 +\n", "expected an expression", 0);
    expect_error(HEAD "encoders:\n  - name: e\n    body: |\n      FROB 1\n", "unknown statement 'FROB'", 0);
    expect_error(HEAD "encoders:\n  - name: e\n    body: |\n      EMIT8 1\n      EMIT_RELOCATION s, R_NOPE\n",
                 "unknown relocation 'R_NOPE'", 0);
    expect_error(HEAD "mnemonics: [{name: m}]\nforms:\n  - {mnemonic: m, encoder: nope}\n",
                 "unknown encoder 'nope'", 0);
    expect_error(HEAD "registers:\n  - {name: r0, class: gpr, width: 32, enc: -1}\n", "below the minimum", 5);
    expect_error("schema: 1\ntargets: [{arch: t, addr_size: 32, byte_order: le}\n", "YAML", 0);
    expect_error(HEAD "features:\n  - {name: a, name: b}\n", "duplicate key", 5);
    expect_error(HEAD "backend: {file: ../etc/passwd}\n", "must match", 4);

    /* encode-time failures */
    DsymDiags d;
    dsym_diags_init(&d);
    const char *doc = HEAD "encoders:\n  - name: r\n    body: |\n      REQUIRE rd <= 31\n"
                           "      EMIT8 rd\n  - name: big\n    body: EMIT8 300\n";
    DsymSpec *s = dsym_spec_load_buffer(doc, strlen(doc), DSYM_FORMAT_YAML, "ok.yaml", 0, &d);
    CHECK(s != NULL, "constraint doc loads");
    if (s) {
        DsymEncoded out;
        const char *n[] = {"rd"};
        int64_t v[] = {40};
        CHECK(!dsym_encode_named(s, "r", 0, n, v, 1, false, &out) && strstr(out.error, "constraint"),
              "REQUIRE enforced");
        v[0] = 7;
        CHECK(dsym_encode_named(s, "r", 0, n, v, 1, false, &out) && out.size == 1 && out.bytes[0] == 7,
              "REQUIRE passes");
        CHECK(!dsym_encode_named(s, "big", 0, NULL, NULL, 0, false, &out) &&
                  strstr(out.error, "does not fit"),
              "EMIT8 overflow");
    }
    dsym_spec_free(s);
    dsym_diags_free(&d);

    /* constant folding: the whole expression collapses to one constant */
    DsymArena *a = dsym_arena_new();
    const DsymProgram *p = dsym_program_compile(a, "EMIT32LE (1 + 2) * 4 - -3 % 2\n", NULL, 0, NULL,
                                                NULL, NULL, NULL);
    CHECK(p && p->code_count == 1 && p->code[0].imm == 11, "constant folding");
    p = dsym_program_compile(a, "EMIT32LE x*2 + 1 << 3\nREQUIRE x >= 0 && x < 8\n", NULL, 0, NULL,
                             NULL, NULL, NULL);
    CHECK(p && p->param_count == 1, "params");
    if (p) {
        int64_t args[] = {5}, r;
        CHECK(dsym_expr_eval(p, p->stmts[0].expr[0], args, &r, NULL) && r == 88, "precedence");
        CHECK(dsym_expr_eval(p, p->stmts[1].expr[0], args, &r, NULL) && r == 1, "logic");
    }
    dsym_arena_free(a);
}

/* ---- 5. catalogue ------------------------------------------------------- */

static void catalogue(const char *cat, size_t expected) {
    DsymDiags d;
    dsym_diags_init(&d);
    ArchDB *db = archdb_open(cat, &d);
    CHECK(db != NULL, "open catalogue %s", cat);
    if (!db) {
        dsym_diags_print(&d, stderr);
        dsym_diags_free(&d);
        return;
    }
    CHECK(archdb_count(db) == expected, "catalogue has %zu entries, directory %zu",
          archdb_count(db), expected);
    for (size_t i = 0; i < archdb_count(db); i++) {
        const ArchDBEntry *e = archdb_entry(db, i);
        const DsymSpec *s = archdb_load(db, e, 0, &d);
        CHECK(s != NULL, "catalogue load %s", e->name);
        if (s) {
            CHECK(e->arch && !strcmp(e->arch, s->targets[0].arch), "%s summary arch is stale",
                  e->name);
            CHECK(archdb_load(db, e, 0, &d) == s, "cache");
        }
    }
    const ArchDBEntry *x = archdb_find(db, "riscv64");
    CHECK(x && !strcmp(x->name, "riscv64"), "find riscv64");
    CHECK(archdb_find(db, "no-such-arch") == NULL, "find missing");
    archdb_close(db);
    dsym_diags_free(&d);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: test_archdb ARCHDB_DIR CATALOGUE\n");
        return 2;
    }
    size_t n = roundtrip_dir(argv[1]);
    CHECK(n > 0, "no specs found");
    roundtrip_trees();
    encodings(argv[1]);
    encoding_table(argv[1]);
    diagnostics();
    biased_lint_tests();
    catalogue(argv[2], n);
    printf("%d check(s), %d failure(s), %zu spec(s) x %zu formats\n", checks, failures, n,
           sizeof FORMATS / sizeof *FORMATS);
    return failures ? 1 : 0;
}
