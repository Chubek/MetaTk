/* Focused checked-encoding tests; no architecture backend data required. */
#include "DSymspec.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(test, label) do { \
    checks++; \
    if (!(test)) { failures++; fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, label); } \
} while (0)

static const char *const base =
    "schema: 1\n"
    "targets:\n"
    "  - {arch: t, addr_size: 32, byte_order: little}\n"
    "  - {arch: t, addr_size: 32, byte_order: big}\n"
    "registers:\n"
    "  - {name: zero, class: gpr, width: 32, enc: 0}\n"
    "  - {name: one, class: gpr, width: 32, enc: 1}\n"
    "  - {name: two64, class: gpr, width: 64, enc: 2}\n"
    "  - {name: threevec, class: vec, width: 128, enc: 3}\n"
    "  - {name: three, class: gpr, width: 32, enc: 3}\n"
    "  - {name: four8, class: gpr, width: 8, enc: 4}\n"
    "mnemonics: [{name: m}]\n"
    "relocs: [{name: R_TEST, width: 32}]\n"
    "encoders:\n"
    "  - name: pair\n"
    "    body: |\n"
    "      EMIT_IMM 2, dst + ((imm + 256) % 256)*256\n"
    "  - {name: scalar, body: EMIT64LE value}\n"
    "  - {name: empty, body: EMIT8 7}\n"
    "  - name: relocated\n"
    "    max_relocs: 1\n"
    "    body: |\n"
    "      EMIT32LE 0\n"
    "      EMIT_RELOCATION symbol, R_TEST\n";

static DsymSpec *make_spec(const char *encoder, const char *operands, uint32_t flags,
                           DsymDiags *d) {
    DsymBuf b;
    dsym_buf_init(&b);
    bool ok = dsym_buf_printf(&b, "%sforms:\n  - mnemonic: m\n    encoder: %s\n    operands: %s\n",
                             base, encoder, operands);
    CHECK(ok, "construct test spec");
    DsymSpec *s = ok ? dsym_spec_load_buffer(b.data, b.len, DSYM_FORMAT_YAML,
                                            "form.yaml", flags, d) : NULL;
    dsym_buf_free(&b);
    return s;
}

static void scalar_ranges(void) {
    const struct {
        const char *operand;
        int64_t lo, hi;
        bool negative_rejected, upper_rejected;
    } cases[] = {
        {"[{kind: imm, width: 8, signed: true}]", -128, 127, true, true},
        {"[{kind: imm, width: 8}]", 0, 255, true, true},
        {"[{kind: imm, width: 1, signed: true}]", -1, 0, true, true},
        {"[{kind: imm, width: 1, signed: false}]", 0, 1, true, true},
        {"[{kind: imm, width: 63, signed: true}]", -(INT64_C(1) << 62),
          (INT64_C(1) << 62) - 1, true, true},
        {"[{kind: imm, width: 63}]", 0, INT64_MAX, true, false},
        {"[{kind: imm, width: 64, signed: true}]", INT64_MIN, INT64_MAX, false, false},
        {"[{kind: imm, width: 64}]", 0, INT64_MAX, true, false},
        {"[{kind: imm, width: 128, signed: true}]", INT64_MIN, INT64_MAX, false, false},
        {"[{kind: imm, width: 128}]", 0, INT64_MAX, true, false},
        {"[{kind: imm, signed: false}]", 0, INT64_MAX, true, false},
        {"[{kind: imm}]", INT64_MIN, INT64_MAX, false, false},
        {"[{kind: disp, width: 16, signed: true, imm_min: -10, imm_max: 10}]",
          -10, 10, true, true},
        {"[{kind: abs, width: 8, imm_min: 2}]", 2, 255, true, true},
        {"[{kind: cond, imm_max: 7}]", INT64_MIN, 7, false, true},
        {"[{kind: reglist, width: 8}]", 0, 255, true, true},
        {"[{kind: mem, width: 8, signed: true}]", -128, 127, true, true},
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        DsymDiags d;
        dsym_diags_init(&d);
        DsymSpec *s = make_spec("scalar", cases[i].operand, 0, &d);
        CHECK(s != NULL, "load scalar range case");
        if (s) {
            DsymEncoded out;
            int64_t v = cases[i].lo;
            CHECK(dsym_encode_form(s, 0, 0, &v, 1, &out) && out.size == 8, "lower bound");
            v = cases[i].hi;
            CHECK(dsym_encode_form(s, 0, 0, &v, 1, &out), "upper bound");
            if (cases[i].negative_rejected) {
                v = cases[i].lo - 1;
                CHECK(!dsym_encode_form(s, 0, 0, &v, 1, &out) &&
                      strstr(out.error, "operand 1 (") && !out.size, "below lower bound");
                CHECK(dsym_encode(s, 1, 0, &v, 1, &out), "low-level API bypasses form range");
            }
            if (cases[i].upper_rejected) {
                v = cases[i].hi + 1;
                CHECK(!dsym_encode_form(s, 0, 0, &v, 1, &out) &&
                      strstr(out.error, "operand 1 (") && !out.size, "above upper bound");
            }
        }
        dsym_spec_free(s);
        dsym_diags_free(&d);
    }
}

static void positional_mapping(void) {
    DsymDiags d;
    dsym_diags_init(&d);
    DsymSpec *s = make_spec("pair",
        "[{kind: implicit}, {kind: reg, hidden: true}, "
        "{kind: reg, class: gpr, width: 32}, {kind: implicit, hidden: false}, "
        "{kind: imm, width: 8, signed: true, imm_min: -10, imm_max: 10}]",
        DSYM_LOAD_LINT | DSYM_LOAD_STRICT, &d);
    CHECK(s && !d.count, "hidden/implicit operands skipped in lint mapping");
    if (s) {
        int64_t args[] = {1, -1};
        DsymEncoded out;
        CHECK(dsym_encode_form(s, 0, 0, args, 2, &out) && out.size == 2 &&
              out.bytes[0] == 1 && out.bytes[1] == 255, "visible order, LE target");
        CHECK(dsym_encode_form(s, 0, 1, args, 2, &out) && out.bytes[0] == 255 &&
              out.bytes[1] == 1, "BE target");
        args[1] = 11;
        CHECK(!dsym_encode_form(s, 0, 0, args, 2, &out) &&
              strstr(out.error, "operand 5 (imm)"), "original operand position in errors");
        CHECK(!dsym_encode_form(s, 0, 0, args, 1, &out) &&
              strstr(out.error, "operand 5 (imm): missing"), "missing visible operand");
        CHECK(!dsym_encode_form(s, 0, 0, args, 3, &out), "extra argument rejected before access");
        CHECK(!dsym_encode_form(s, 0, 2, args, 2, &out), "invalid target");
        CHECK(!dsym_encode_form(s, SIZE_MAX, 0, args, 2, &out), "invalid form");
        CHECK(!dsym_encode_form(NULL, 0, 0, args, 2, &out), "null spec");
        CHECK(!dsym_encode_form(s, 0, 0, NULL, 2, &out), "null argument array");
        CHECK(!dsym_encode_form(s, 0, 0, args, 2, NULL), "null output");
        CHECK(!dsym_encode(s, 0, 0, NULL, 2, &out), "low-level null argument array");
        CHECK(!dsym_encode(s, 0, 0, args, 2, NULL), "low-level null output");
        CHECK(!dsym_encode_named(s, "pair", 0, NULL, args, 2, false, &out), "named null arrays");
        CHECK(!dsym_encode_named(s, "pair", 0, NULL, NULL, 0, false, NULL), "named null output");
    }
    dsym_spec_free(s);
    dsym_diags_free(&d);
}

static void registers(void) {
    const struct { const char *operand; int64_t good, bad_width, bad_class; } cases[] = {
        {"[{kind: reg, class: gpr, width: 32}]", 3, 2, 4},
        {"[{kind: reg, class: gpr, width: 64}]", 2, 0, 3},
        {"[{kind: reg, class: vec, width: 128}]", 3, 2, 0},
        {"[{kind: reg, class: vec}]", 3, 2, 0},
        {"[{kind: reg, width: 8}]", 4, 2, 0},
        {"[{kind: reg}]", 2, 99, -1},
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        DsymDiags d;
        dsym_diags_init(&d);
        DsymSpec *s = make_spec("scalar", cases[i].operand, 0, &d);
        CHECK(s != NULL, "load register case");
        if (s) {
            DsymEncoded out;
            int64_t v = cases[i].good;
            CHECK(dsym_encode_form(s, 0, 0, &v, 1, &out), "register encoding matches metadata");
            v = cases[i].bad_width;
            CHECK(!dsym_encode_form(s, 0, 0, &v, 1, &out) &&
                  strstr(out.error, "operand 1 (reg)"), "register width or encoding mismatch");
            v = cases[i].bad_class;
            CHECK(!dsym_encode_form(s, 0, 0, &v, 1, &out), "register class or width mismatch");
            v = 99;
            CHECK(!dsym_encode_form(s, 0, 0, &v, 1, &out), "unknown register encoding");
        }
        dsym_spec_free(s);
        dsym_diags_free(&d);
    }
}

static void mapping_diagnostics(void) {
    const struct { const char *encoder, *operands, *code; bool valid; } cases[] = {
        {"empty", "[]", NULL, true},
        {"empty", "[{kind: implicit}, {kind: reg, hidden: true}]", NULL, true},
        {"pair", "[{kind: mem}]", "DSYM-W002", false},
        {"scalar", "[]", "DSYM-W002", false},
        {"relocated", "[{kind: sym, width: 64}]", NULL, true},
        {"relocated", "[{kind: rel, width: 32}]", NULL, true},
        {"relocated", "[{kind: imm}]", "DSYM-W003", false},
        {"scalar", "[{kind: sym}]", "DSYM-W003", false},
        {"scalar", "[{kind: rel}]", "DSYM-W003", false},
    };
    const uint32_t flags[] = {0, DSYM_LOAD_LINT, DSYM_LOAD_LINT | DSYM_LOAD_STRICT};
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        for (size_t j = 0; j < sizeof flags / sizeof *flags; j++) {
            DsymDiags d;
            dsym_diags_init(&d);
            DsymSpec *s = make_spec(cases[i].encoder, cases[i].operands, flags[j], &d);
            bool strict_failure = !cases[i].valid && (flags[j] & DSYM_LOAD_STRICT);
            CHECK((s != NULL) == !strict_failure, "lint mapping load policy");
            CHECK(d.count == (size_t)(!cases[i].valid && flags[j]), "mapping lint count");
            if (d.count) {
                CHECK(cases[i].code && strstr(d.items[0].msg, cases[i].code), "stable lint code");
                CHECK(d.items[0].severity == (strict_failure ? DSYM_ERROR : DSYM_WARNING),
                      "strict promotes mapping lint");
                CHECK(d.items[0].path && strstr(d.items[0].path, "/forms/0/operands"),
                      "mapping diagnostic path");
            }
            if (s) {
                DsymEncoded out;
                int64_t v = 1;
                size_t n = !strcmp(cases[i].encoder, "empty") ? 0 : 1;
                CHECK(dsym_encode_form(s, 0, 0, n ? &v : NULL, n, &out) == cases[i].valid,
                      "checked encoding enforces mapping even without lint");
            }
            dsym_spec_free(s);
            dsym_diags_free(&d);
        }
    }
    DsymDiags d;
    dsym_diags_init(&d);
    DsymSpec *s = make_spec("scalar", "[{kind: imm}]", DSYM_LOAD_NO_COMPILE | DSYM_LOAD_LINT, &d);
    CHECK(s && !d.count, "no-compile skips mapping lint");
    if (s) {
        int64_t v = 0;
        DsymEncoded out;
        CHECK(!dsym_encode_form(s, 0, 0, &v, 1, &out) && strstr(out.error, "not compiled"),
              "uncompiled encoder rejected");
    }
    dsym_spec_free(s);
    dsym_diags_free(&d);
}

int main(void) {
    scalar_ranges();
    positional_mapping();
    registers();
    mapping_diagnostics();
    printf("%u form-encoding check(s), %u failure(s)\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
