/* Relocation containment, bit placement and endian metadata regression tests. */
#include "DSymspec.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(test, label) do { \
    checks++; \
    if (!(test)) { failures++; fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, label); } \
} while (0)

static DsymSpec *make_spec(const char *statement, int64_t width, int64_t offset,
                           bool has_offset, uint32_t flags, DsymDiags *d) {
    DsymBuf b;
    dsym_buf_init(&b);
    dsym_buf_printf(&b,
        "schema: 1\n"
        "targets:\n"
        "  - {arch: test, addr_size: 32, byte_order: little}\n"
        "  - {arch: test, addr_size: 32, byte_order: big}\n"
        "relocs:\n"
        "  - {name: R_TEST, width: %" PRId64, width);
    if (has_offset) dsym_buf_printf(&b, ", offset: %" PRId64, offset);
    dsym_buf_printf(&b,
        "}\nencoders:\n"
        "  - name: test\n"
        "    max_relocs: 1\n"
        "    body: |\n"
        "      EMIT8 0xAA\n"
        "      %s\n"
        "      REQUIRE 1\n"
        "      EMIT_RELOCATION symbol, R_TEST\n", statement);
    CHECK(!b.oom, "construct relocation document");
    DsymSpec *s = dsym_spec_load_buffer(b.data, b.len, DSYM_FORMAT_YAML,
                                       "reloc.yaml", flags, d);
    dsym_buf_free(&b);
    return s;
}

static void containment(void) {
    const struct {
        const char *emit;
        int64_t width, offset;
        bool has_offset, valid;
        uint32_t size;
    } cases[] = {
        {"EMIT32LE 0", 32, 0, false, true, 4},
        {"EMIT32BE 0", 26, 0, false, true, 4},
        {"EMIT32LE 0", 26, 6, true, true, 4},
        {"EMIT32LE 0", 26, 7, true, false, 4},
        {"EMIT8 0", 32, 0, false, false, 1},
        {"EMIT8 0", 1, 7, true, true, 1},
        {"EMIT8 0", 1, 8, true, false, 1},
        {"EMIT16BE 0", 16, 0, true, true, 2},
        {"EMIT64LE 0", 64, 0, true, true, 8},
        {"EMIT64BE 0", 64, 1, true, false, 8},
        {"EMIT64LE 0", INT64_MAX, 0, false, false, 8},
        {"EMIT64LE 0", 1, INT64_MAX, true, false, 8},
        {"EMIT64LE 0", INT64_MAX, INT64_MAX, true, false, 8},
        {"EMIT_MODRM 0, 0", 8, 0, false, true, 1},
        {"EMIT_MODRM 0, 0", 9, 0, false, false, 1},
        {"EMIT_IMM 4, 0", 26, 6, true, true, 4},
    };
    const uint32_t flags[] = {0, DSYM_LOAD_LINT, DSYM_LOAD_STRICT | DSYM_LOAD_LINT};
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        for (size_t j = 0; j < sizeof flags / sizeof *flags; j++) {
            DsymDiags d;
            dsym_diags_init(&d);
            DsymSpec *s = make_spec(cases[i].emit, cases[i].width, cases[i].offset,
                                    cases[i].has_offset, flags[j], &d);
            bool strict_error = !cases[i].valid && (flags[j] & DSYM_LOAD_STRICT);
            CHECK((s != NULL) == !strict_error, "relocation containment loading policy");
            CHECK(d.count == (size_t)(!cases[i].valid && flags[j]), "relocation lint count");
            if (d.count) {
                CHECK(strstr(d.items[0].msg, "DSYM-W004") &&
                      strstr(d.items[0].msg, "statement 4") &&
                      strstr(d.items[0].msg, "R_TEST"), "relocation diagnostic context");
                CHECK(d.items[0].severity == (strict_error ? DSYM_ERROR : DSYM_WARNING),
                      "relocation strict promotion");
            }
            if (s) {
                DsymEncoded out;
                int64_t symbol = 17;
                CHECK(dsym_encode(s, 0, 0, &symbol, 1, &out) == cases[i].valid,
                      "runtime refuses non-fitting relocation even without lint");
                if (cases[i].valid) {
                    CHECK(out.reloc_count == 1 && out.relocs[0].offset == 1 &&
                          out.relocs[0].size == cases[i].size &&
                          out.relocs[0].bit_width == (uint32_t)cases[i].width &&
                          out.relocs[0].bit_offset == (uint32_t)(cases[i].has_offset ? cases[i].offset : 0) &&
                          out.relocs[0].sym_value == 17, "complete relocation metadata");
                } else {
                    CHECK(strstr(out.error, "R_TEST") && !out.reloc_count,
                          "failed relocation not appended");
                }
            }
            dsym_spec_free(s);
            dsym_diags_free(&d);
        }
    }
}

static void endian_placement(void) {
    const char *const emits[] = {"EMIT32LE 0xA0000005", "EMIT32BE 0xA0000005",
                                 "EMIT_IMM 4, 0xA0000005"};
    for (size_t i = 0; i < sizeof emits / sizeof *emits; i++) {
        DsymDiags d;
        dsym_diags_init(&d);
        DsymSpec *s = make_spec(emits[i], 20, 5, true, DSYM_LOAD_LINT | DSYM_LOAD_STRICT, &d);
        CHECK(s != NULL && !d.count, "load partial bit-field relocation");
        if (s) {
            for (size_t target = 0; target < 2; target++) {
                DsymEncoded out;
                int64_t symbol = 42;
                bool ok = dsym_encode(s, 0, target, &symbol, 1, &out);
                CHECK(ok, "encode endian relocation");
                if (!ok) continue;
                const DsymEncodedReloc *r = &out.relocs[0];
                bool be = i == 1 || (i == 2 && target == 1);
                CHECK(r->endian == (be ? DSYM_ENDIAN_BIG : DSYM_ENDIAN_LITTLE),
                      "actual endian, not TARGET or default target endian");
                /* Demonstrate linker insertion using only returned layout metadata. */
                uint64_t field = 0;
                for (uint32_t k = 0; k < r->size; k++) {
                    unsigned shift = (r->endian == DSYM_ENDIAN_BIG ? r->size - 1 - k : k) * 8;
                    field |= (uint64_t)out.bytes[r->offset + k] << shift;
                }
                CHECK(field == UINT64_C(0xA0000005), "read emitted integer in declared order");
                uint64_t mask = ((UINT64_C(1) << r->bit_width) - 1) << r->bit_offset;
                field = (field & ~mask) | ((UINT64_C(0xABCDE) << r->bit_offset) & mask);
                CHECK(field == UINT64_C(0xA1579BC5), "bit offset honored without touching opcode");
            }
        }
        dsym_spec_free(s);
        dsym_diags_free(&d);
    }
}

static void multiple_fields(void) {
    const char *doc =
        "schema: 1\n"
        "targets: [{arch: t, addr_size: 32, byte_order: little}]\n"
        "relocs: [{name: R1, width: 3, offset: 5}, {name: R2, width: 12, offset: 4}]\n"
        "encoders:\n"
        "  - name: e\n"
        "    max_relocs: 3\n"
        "    body: |\n"
        "      EMIT8 0\n"
        "      EMIT_RELOCATION first, R1\n"
        "      EMIT_RELOCATION second, R1\n"
        "      EMIT16BE 0\n"
        "      EMIT_RELOCATION third, R2\n";
    DsymDiags d;
    dsym_diags_init(&d);
    DsymSpec *s = dsym_spec_load_buffer(doc, strlen(doc), DSYM_FORMAT_YAML, "multi.yaml",
                                       DSYM_LOAD_LINT | DSYM_LOAD_STRICT, &d);
    CHECK(s && !d.count, "multiple relocation fields load");
    if (s) {
        DsymEncoded out;
        int64_t values[] = {7, 8, 9};
        CHECK(dsym_encode(s, 0, 0, values, 3, &out) && out.reloc_count == 3,
              "multiple relocations encoded");
        CHECK(out.relocs[0].offset == 0 && out.relocs[1].offset == 0 &&
              out.relocs[2].offset == 1 && out.relocs[2].size == 2 &&
              out.relocs[2].bit_offset == 4 && out.relocs[2].bit_width == 12 &&
              out.relocs[2].endian == DSYM_ENDIAN_BIG && out.relocs[2].sym_param == 2,
              "relocations follow latest field, including repeat and offset changes");
    }
    dsym_spec_free(s);
    dsym_diags_free(&d);
}

int main(void) {
    containment();
    endian_placement();
    multiple_fields();
    printf("%u relocation check(s), %u failure(s)\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
