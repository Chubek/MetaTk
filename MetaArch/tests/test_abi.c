#include "MetaArchABI.h"
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(x) do { checks++; if (!(x)) { failures++; fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    CHECK(metaarch_abi_version() == METAARCH_ABI_VERSION);
    MaDiagnostics *d = metaarch_diagnostics_create();
    CHECK(d != NULL);
    if (!d) return 1;
    MaSpec *s = metaarch_spec_load(argv[1], MA_AUTO, MA_LOAD_LINT | MA_LOAD_STRICT, d);
    CHECK(s != NULL && metaarch_diagnostics_count(d) == 0);
    if (!s) { metaarch_diagnostics_free(d); return 1; }
    CHECK(metaarch_spec_target_count(s) == 2 && metaarch_spec_register_count(s) == 2);
    CHECK(metaarch_spec_encoder_count(s) == 3 && metaarch_spec_form_count(s) == 3);
    MaTargetView target;
    CHECK(metaarch_spec_target(s, 0, &target) == MA_OK && !strcmp(target.arch, "test") &&
          !strcmp(target.subarch, "v1") && !strcmp(target.object_format, "ELF32") &&
          !strcmp(target.abi, "test") && target.address_width == 32 && target.byte_order == MA_LITTLE_ENDIAN);
    CHECK(metaarch_spec_target(s, 1, &target) == MA_OK && target.subarch == NULL && target.byte_order == MA_BIG_ENDIAN);
    MaRegisterView reg;
    CHECK(metaarch_spec_register(s, 1, &reg) == MA_OK && !strcmp(reg.name, "r1") &&
          !strcmp(reg.reg_class, "gpr") && reg.encoding == 1 && reg.width == 32);
    CHECK(metaarch_spec_find_register(s, "r1") == 1 && metaarch_spec_find_register(s, "missing") == -1);
    CHECK(metaarch_spec_find_encoder(s, "branch") == 1 && !strcmp(metaarch_spec_encoder_name(s, 2), "ret"));
    MaFormView form;
    CHECK(metaarch_spec_form(s, 0, &form) == MA_OK && form.operand_count == 3 &&
          form.visible_operand_count == 2 && !strcmp(form.mnemonic, "pair") && !strcmp(form.encoder, "pair"));
    const char *names[] = {"dst", "imm"};
    int64_t values[] = {1, -1};
    MaResult *r = metaarch_spec_encode(s, "pair", 0, names, values, 2, d);
    CHECK(r && metaarch_result_size(r) == 4 && !memcmp(metaarch_result_data(r), "\x01\xff\x00\x00", 4));
    CHECK(metaarch_result_relocation_count(r) == 0);
    metaarch_result_free(r);
    r = metaarch_spec_encode_form(s, 0, 1, values, 2, d);
    CHECK(r && !memcmp(metaarch_result_data(r), "\x00\x00\xff\x01", 4));
    metaarch_result_free(r);
    values[1] = 11;
    CHECK(!metaarch_spec_encode_form(s, 0, 0, values, 2, d));
    MaDiagnosticView diag;
    CHECK(metaarch_diagnostics_get(d, 0, &diag) == MA_OK && diag.severity == MA_ERROR &&
          strstr(diag.message, "operand 3 (imm)"));
    metaarch_diagnostics_clear(d);
    CHECK(!metaarch_spec_encode(s, "pair", 0, NULL, values, 2, d));
    CHECK(!metaarch_spec_encode(s, "pair", 0, names, NULL, 2, d));
    CHECK(!metaarch_spec_encode(s, "pair", 0, names, values, 65, d));
    const char *bad_names[] = {NULL, "imm"};
    CHECK(!metaarch_spec_encode(s, "pair", 0, bad_names, values, 2, d));
    const char *duplicate_names[] = {"dst", "dst"};
    CHECK(!metaarch_spec_encode(s, "pair", 0, duplicate_names, values, 2, d));
    CHECK(!metaarch_spec_encode(s, "pair", SIZE_MAX, names, values, 2, d));
    CHECK(!metaarch_spec_encode(s, "absent", 0, NULL, NULL, 0, d));
    CHECK(!metaarch_spec_encode_form(s, SIZE_MAX, 0, NULL, 0, d));
    CHECK(!metaarch_spec_write(s, MA_AUTO, 0, d));
    CHECK(!metaarch_spec_write(s, MA_JSON, 128, d));
    r = metaarch_spec_encode(s, "ret", 0, NULL, NULL, 0, d);
    CHECK(r && metaarch_result_size(r) == 1 && metaarch_result_data(r)[0] == 0xC3);
    metaarch_result_free(r);
    CHECK(metaarch_spec_target(s, SIZE_MAX, &target) == MA_NOT_FOUND);
    CHECK(metaarch_spec_register(s, SIZE_MAX, &reg) == MA_NOT_FOUND);
    CHECK(metaarch_spec_form(s, SIZE_MAX, &form) == MA_NOT_FOUND);
    CHECK(!metaarch_spec_encoder_name(s, SIZE_MAX));
    metaarch_diagnostics_clear(d);
    for (MaFormat f = MA_YAML; f <= MA_SEXPR; f++) {
        r = metaarch_spec_write(s, f, MA_WRITE_REF_BACKEND, d);
        CHECK(r && metaarch_result_size(r) > 0);
        MaSpec *copy = r ? metaarch_spec_parse(metaarch_result_data(r), metaarch_result_size(r), f,
                                              "roundtrip", 0, d) : NULL;
        metaarch_result_free(r);
        CHECK(copy && metaarch_spec_form_count(copy) == 3);
        metaarch_spec_free(copy);
    }
    CHECK(metaarch_spec_load_backend(s, d));
    values[0] = 42;
    const char *symbol[] = {"symbol"};
    r = metaarch_spec_encode(s, "branch", 0, symbol, values, 1, d);
    CHECK(r && metaarch_result_relocation_count(r) == 1);
    metaarch_spec_free(s);
    MaRelocationView relocation;
    CHECK(metaarch_result_relocation(r, 0, &relocation) == MA_OK && !strcmp(relocation.name, "R_FIX") &&
          relocation.offset == 1 && relocation.size == 4 && relocation.bit_offset == 5 &&
          relocation.bit_width == 20 && relocation.byte_order == MA_BIG_ENDIAN &&
          relocation.symbol_parameter == 0 && relocation.symbol_value == 42);
    CHECK(metaarch_result_size(r) == 5 && metaarch_result_data(r)[0] == 0xAA);
    CHECK(metaarch_result_relocation(r, 1, &relocation) == MA_NOT_FOUND);
    metaarch_result_free(r);

    CHECK(!metaarch_spec_load(NULL, MA_AUTO, 0, d));
    CHECK(!metaarch_spec_load(argv[1], -1, 0, d));
    CHECK(!metaarch_spec_load(argv[1], MA_AUTO, 128, d));
    CHECK(!metaarch_spec_parse(NULL, 1, MA_YAML, NULL, 0, d));
    CHECK(!metaarch_spec_parse("bad: [", 6, MA_YAML, "bad.yaml", 0, d));
    CHECK(!metaarch_spec_write(NULL, MA_JSON, 0, d));
    CHECK(!metaarch_spec_encode_form(NULL, 0, 0, NULL, 0, d));
    CHECK(metaarch_spec_target(NULL, 0, &target) == MA_INVALID_ARGUMENT);
    CHECK(metaarch_spec_register(NULL, 0, NULL) == MA_INVALID_ARGUMENT);
    CHECK(metaarch_result_relocation(NULL, 0, &relocation) == MA_INVALID_ARGUMENT);
    CHECK(metaarch_diagnostics_get(d, SIZE_MAX, &diag) == MA_NOT_FOUND);
    CHECK(metaarch_result_size(NULL) == 0 && !metaarch_result_data(NULL));
    CHECK(metaarch_spec_find_encoder(NULL, NULL) == -1 && metaarch_spec_target_count(NULL) == 0);
    metaarch_spec_free(NULL); metaarch_result_free(NULL); metaarch_diagnostics_clear(NULL);
    metaarch_diagnostics_free(d); metaarch_diagnostics_free(NULL);
    printf("%u C ABI checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
