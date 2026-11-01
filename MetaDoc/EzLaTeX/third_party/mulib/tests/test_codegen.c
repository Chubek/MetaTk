#define MULIB_IMPL_DSYMSPEC
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_LINKER
#define MULIB_IMPL_ASSEMBLER
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#include "mulib-assembler.h"
#include "mulib-jit.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *source_file(const char *name, size_t *length)
{
    char path[2048];
    const char *slash = strrchr(__FILE__, '/');
    size_t prefix = slash ? (size_t)(slash - __FILE__) : 1;
    const char *directory = slash ? __FILE__ : ".";
    int written;
    FILE *file;
    long size;
    char *text;
    assert(prefix < 1024);
    written = snprintf(path, sizeof(path), "%.*s/../dsymspec/%s", (int)prefix, directory, name);
    assert(written > 0 && (size_t)written < sizeof(path));
    file = fopen(path, "rb"); assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    size = ftell(file); assert(size >= 0 && size < 4 * 1024 * 1024);
    assert(fseek(file, 0, SEEK_SET) == 0);
    *length = (size_t)size;
    text = (char *)malloc(*length + 1); assert(text);
    assert(fread(text, 1, *length, file) == *length);
    assert(fclose(file) == 0);
    text[*length] = 0;
    return text;
}

static void invalid_metadata(const char *text)
{
    mulib_dsymspec_error_t error;
    assert(!mulib_dsymspec_load_json(text, strlen(text), &error));
    assert(error.message);
}

static mulib_dsymspec_t *metadata(void)
{
    size_t i, length;
    mulib_dsymspec_error_t error;
    mulib_dsymspec_t *spec, *copy;
    const mulib_json_t *record, *doc, *properties;
    mulib_json_t *schema;
    char *text;
    assert(mulib_dsymspec_builtin_count() == 28);
    for (i = 0; i < mulib_dsymspec_builtin_count(); ++i) {
        spec = mulib_dsymspec_load_builtin(mulib_dsymspec_builtin_name(i), &error);
        assert(spec && !error.message);
        assert(mulib_dsymspec_count(spec, "targets") > 0);
        mulib_dsymspec_destroy(spec);
    }
    assert(!mulib_dsymspec_builtin_name(SIZE_MAX));
    assert(!mulib_dsymspec_load_builtin("not-an-architecture", &error) && error.message);
    assert(!mulib_dsymspec_load_builtin(NULL, &error));
    spec = mulib_dsymspec_load_builtin("amd64", &error); assert(spec);
    record = mulib_dsymspec_find(spec, "targets", "amd64"); assert(record);
    assert(mulib_json_numeric(mulib_json_get(record, "addr_size")) == 64);
    assert(!strcmp(mulib_json_text(mulib_json_get(record, "calling_conv"), NULL), "sysv-amd64"));
    record = mulib_dsymspec_find(spec, "registers", "rdi"); assert(record);
    assert(mulib_json_numeric(mulib_json_get(record, "enc")) == 7);
    assert(mulib_json_numeric(mulib_json_get(record, "width")) == 64);
    record = mulib_dsymspec_find(spec, "encoders", "mov"); assert(record);
    assert(!strcmp(mulib_json_text(mulib_json_get(record, "body"), NULL),
                   "EMIT8 0x48\nEMIT8 0x89\nEMIT_MODRM src, dst\n"));
    assert(mulib_dsymspec_find(spec, "forms", "mov"));
    assert(!mulib_dsymspec_find(spec, "registers", "unknown"));
    assert(!mulib_dsymspec_record(spec, "registers", SIZE_MAX));
    assert(!mulib_dsymspec_record(NULL, NULL, 0));
    assert(!mulib_dsymspec_find(NULL, NULL, NULL));
    assert(!mulib_dsymspec_count(spec, "schema"));
    doc = mulib_dsymspec_document(spec);
    record = mulib_json_get(doc, "_mulib_codegen");
    assert(strlen(mulib_json_text(mulib_json_get(record, "source_sha256"), NULL)) == 64);
    assert(strlen(mulib_json_text(mulib_json_get(record, "schema_sha256"), NULL)) == 64);
    assert(mulib_json_size(mulib_dsymspec_encodings(spec)) > 20);
    record = mulib_json_get(mulib_dsymspec_find(spec, "forms", "movabs"), "operands");
    assert(!strcmp(mulib_json_text(mulib_json_get(mulib_json_at(record, 1), "imm_max"), NULL), "9223372036854775807"));
    text = mulib_json_stringify(doc, 0); assert(text);
    copy = mulib_dsymspec_load_json(text, strlen(text), &error); assert(copy);
    memset(text, 0, strlen(text)); free(text);
    assert(mulib_dsymspec_find(copy, "registers", "rax"));
    mulib_dsymspec_destroy(copy);
    text = source_file("amd64.dsymspec.yaml", &length);
    assert(strstr(text, "EMIT_MODRM src, dst"));
    assert(strstr(text, "mnemonic: mov, form: RegisterRegister, widths: [32, 64], encoder: mov"));
    free(text);
    text = source_file("schema/dsymspec.schema.json", &length);
    schema = mulib_json_parse(text, length, NULL); free(text); assert(schema);
    properties = mulib_json_get(schema, "properties");
    assert(mulib_json_numeric(mulib_json_get(mulib_json_get(properties, "schema"), "const")) == 1);
    assert(mulib_json_get(properties, "asmtk"));
    assert(mulib_json_get(properties, "encoders"));
    assert(!mulib_json_boolean(mulib_json_get(schema, "additionalProperties")));
    mulib_json_destroy(schema);
    invalid_metadata(""); invalid_metadata("[]"); invalid_metadata("{");
    invalid_metadata("{\"schema\":1,\"schema\":1,\"targets\":[]}");
    invalid_metadata("{\"schema\":2,\"targets\":[]}");
    invalid_metadata("{\"schema\":1,\"targets\":[{\"arch\":\"x\",\"addr_size\":64,\"byte_order\":\"little\"},{\"arch\":\"x\",\"addr_size\":32,\"byte_order\":\"big\"}]}");
    invalid_metadata("{\"schema\":1,\"targets\":[{\"arch\":\"x\",\"addr_size\":64,\"byte_order\":\"middle\"}]}");
    invalid_metadata("{\"schema\":1,\"targets\":[{\"arch\":\"x\",\"addr_size\":64,\"byte_order\":\"little\",\"align\":3}]}");
    invalid_metadata("{\"schema\":1,\"targets\":[{\"arch\":\"x\\u0000y\",\"addr_size\":64,\"byte_order\":\"little\"}]}");
    invalid_metadata("{\"schema\":1,\"targets\":[{\"arch\":\"x\",\"addr_size\":64,\"byte_order\":\"little\"}],\"registers\":[{\"name\":\"r\",\"class\":\"gpr\",\"width\":3,\"enc\":0}]}");
    invalid_metadata("{\"schema\":1,\"targets\":[{\"arch\":\"x\",\"addr_size\":64,\"byte_order\":\"little\"}],\"mnemonics\":[{\"name\":\"n\"},{\"name\":\"n\"}]}");
    invalid_metadata("{\"schema\":1,\"targets\":[{\"arch\":\"x\",\"addr_size\":64,\"byte_order\":\"little\"}],\"forms\":[{\"mnemonic\":\"n\",\"encoder\":\"missing\"}]}");
    assert(!mulib_dsymspec_load_json(NULL, 1, &error));
    assert(!mulib_dsymspec_load_json("", SIZE_MAX, &error));
    mulib_dsymspec_destroy(NULL);
    return spec;
}

static void emitter(void)
{
    unsigned char storage[32] = {0}, snapshot[32];
    mulib_emitter_t e = MULIB_EMITTER_INIT;
    size_t offset = SIZE_MAX;
    assert(mulib_emitter_init(&e, NULL, 0, MULIB_EMITTER_LITTLE) == 0);
    assert(mulib_emitter_bytes(&e, NULL, 0) == 0);
    assert(mulib_emitter_patch(&e, 0, NULL, 0) == 0);
    assert(mulib_emitter_reserve(&e, 0, &offset) == 0 && offset == 0);
    assert(mulib_emitter_uint(&e, 1, 1) == MULIB_EMITTER_ENOSPACE);
    assert(mulib_emitter_init(&e, storage, sizeof(storage), MULIB_EMITTER_LITTLE) == 0);
    assert(mulib_emitter_uint(&e, UINT64_C(0x0102030405060708), 8) == 0);
    assert(!memcmp(storage, "\x08\x07\x06\x05\x04\x03\x02\x01", 8));
    assert(mulib_emitter_patch_uint(&e, 2, 0xabcd, 2) == 0);
    assert(storage[2] == 0xcd && storage[3] == 0xab);
    assert(mulib_emitter_bytes(&e, storage, 4) == 0 && e.size == 12);
    assert(mulib_emitter_patch(&e, 1, storage, 4) == 0);
    assert(mulib_emitter_reserve(&e, 20, &offset) == 0 && offset == 12 && e.size == 32);
    memcpy(snapshot, storage, sizeof(storage));
    assert(mulib_emitter_uint(&e, 256, 1) == MULIB_EMITTER_ERANGE);
    assert(mulib_emitter_uint(&e, 0, 0) == MULIB_EMITTER_EINVAL);
    assert(mulib_emitter_uint(&e, 0, 9) == MULIB_EMITTER_EINVAL);
    assert(mulib_emitter_reserve(&e, SIZE_MAX, &offset) == MULIB_EMITTER_ENOSPACE && offset == 12);
    assert(mulib_emitter_patch_uint(&e, SIZE_MAX, 1, 1) == MULIB_EMITTER_ERANGE);
    assert(mulib_emitter_patch(&e, 31, storage, 2) == MULIB_EMITTER_ERANGE);
    assert(mulib_emitter_bytes(&e, NULL, 1) == MULIB_EMITTER_EINVAL);
    assert(!memcmp(snapshot, storage, sizeof(storage)) && e.size == 32);
    assert(mulib_emitter_init(&e, storage, sizeof(storage), MULIB_EMITTER_BIG) == 0);
    assert(mulib_emitter_uint(&e, 0x123456, 3) == 0);
    assert(!memcmp(storage, "\x12\x34\x56", 3));
    assert(mulib_emitter_init(NULL, storage, 32, MULIB_EMITTER_BIG) == MULIB_EMITTER_EINVAL);
    assert(mulib_emitter_init(&e, NULL, 1, MULIB_EMITTER_BIG) == MULIB_EMITTER_EINVAL);
}

static void linker(void)
{
    unsigned char bytes[24], original[24];
    mulib_emitter_t e;
    mulib_linker_t *l;
    size_t bad = 0;
    assert(mulib_emitter_init(&e, bytes, sizeof(bytes), MULIB_EMITTER_BIG) == 0);
    assert(mulib_emitter_reserve(&e, sizeof(bytes), NULL) == 0);
    l = mulib_linker_create(3, 3); assert(l);
    assert(mulib_linker_define(l, "absolute", UINT64_C(0x12345678), 1) == 0);
    assert(mulib_linker_define(l, "absolute", 1, 1) == MULIB_LINKER_EDUPLICATE);
    assert(mulib_linker_relocate(l, "absolute", 0, MULIB_LINKER_ABS32, 1) == 0);
    assert(mulib_linker_relocate(l, "missing", 8, MULIB_LINKER_ABS64, 0) == 0);
    memcpy(original, bytes, sizeof(bytes));
    assert(mulib_linker_apply(l, &e, 4096, &bad) == MULIB_LINKER_EUNDEFINED && bad == 1);
    assert(!memcmp(bytes, original, sizeof(bytes)));
    assert(mulib_linker_define(l, "missing", 16, 0) == 0);
    assert(mulib_linker_apply(l, &e, 4096, &bad) == 0 && bad == SIZE_MAX);
    assert(!memcmp(bytes, "\x12\x34\x56\x79", 4));
    assert(!memcmp(bytes + 8, "\0\0\0\0\0\0\x10\x10", 8));
    assert(mulib_linker_relocate(l, "other", 10, MULIB_LINKER_ABS32, 0) == MULIB_LINKER_EOVERLAP);
    assert(mulib_linker_symbol_count(l) == 2 && mulib_linker_relocation_count(l) == 2);
    assert(mulib_linker_relocate(l, "other", SIZE_MAX, MULIB_LINKER_ABS32, 0) == MULIB_LINKER_ERANGE);
    assert(mulib_linker_define(l, "bad name", 0, 0) == MULIB_LINKER_EINVAL);
    assert(mulib_linker_relocate(l, "other", 20, (mulib_linker_kind)99, 0) == MULIB_LINKER_EINVAL);
    assert(mulib_linker_apply(l, &e, UINT64_MAX, &bad) == MULIB_LINKER_ERANGE && bad == SIZE_MAX);
    mulib_linker_destroy(l);
    l = mulib_linker_create(1, 1); assert(l);
    assert(mulib_emitter_init(&e, bytes, sizeof(bytes), MULIB_EMITTER_LITTLE) == 0);
    assert(mulib_emitter_reserve(&e, 8, NULL) == 0);
    assert(mulib_linker_define(l, "zero", 0, 0) == 0);
    assert(mulib_linker_relocate(l, "zero", 1, MULIB_LINKER_PC32, -4) == 0);
    assert(mulib_linker_apply(l, &e, 0, NULL) == 0);
    assert(!memcmp(bytes + 1, "\xfb\xff\xff\xff", 4));
    assert(mulib_linker_define(l, "overflow", 0, 0) == MULIB_LINKER_ELIMIT);
    assert(mulib_linker_relocate(l, "zero", 4, MULIB_LINKER_ABS32, 0) == MULIB_LINKER_ELIMIT);
    mulib_linker_destroy(l);
    l = mulib_linker_create(1, 1); assert(l);
    assert(mulib_linker_define(l, "far", UINT64_C(2147483648), 1) == 0);
    assert(mulib_linker_relocate(l, "far", 0, MULIB_LINKER_PC32, 0) == 0);
    memset(bytes, 0xa5, sizeof(bytes));
    assert(mulib_linker_apply(l, &e, 0, &bad) == MULIB_LINKER_ERANGE && bad == 0 && bytes[0] == 0xa5);
    mulib_linker_destroy(l);
    l = mulib_linker_create(1, 1); assert(l);
    assert(mulib_linker_define(l, "low", 0, 1) == 0);
    assert(mulib_linker_relocate(l, "low", 0, MULIB_LINKER_PC32, 0) == 0);
    assert(mulib_linker_apply(l, &e, UINT64_C(2147483648), NULL) == 0);
    assert(!memcmp(bytes, "\0\0\0\x80", 4));
    assert(mulib_linker_apply(l, &e, UINT64_C(2147483649), NULL) == MULIB_LINKER_ERANGE);
    mulib_linker_destroy(l);
    l = mulib_linker_create(1, 1); assert(l);
    assert(mulib_linker_define(l, "huge", UINT64_C(0x8000000000000000), 1) == 0);
    assert(mulib_linker_relocate(l, "huge", 0, MULIB_LINKER_PC32, INT64_MIN) == 0);
    assert(mulib_linker_apply(l, &e, 0, NULL) == 0 && bytes[0] == 0);
    mulib_linker_destroy(l);
    l = mulib_linker_create(1, 1); assert(l);
    assert(mulib_linker_define(l, "wide", UINT64_MAX, 1) == 0);
    assert(mulib_linker_relocate(l, "wide", 0, MULIB_LINKER_ABS64, 1) == 0);
    assert(mulib_linker_apply(l, &e, 0, NULL) == MULIB_LINKER_ERANGE);
    mulib_linker_destroy(l);
    l = mulib_linker_create(0, 0); assert(l);
    assert(mulib_linker_define(l, "x", 0, 0) == MULIB_LINKER_ELIMIT);
    assert(mulib_linker_apply(l, &e, 0, NULL) == 0);
    mulib_linker_destroy(l);
    l = mulib_linker_create(1, 1); assert(l);
    assert(mulib_linker_define(l, "too-wide", UINT64_C(0x100000000), 1) == 0);
    assert(mulib_linker_relocate(l, "too-wide", 0, MULIB_LINKER_ABS32, 0) == 0);
    assert(mulib_linker_apply(l, &e, 0, &bad) == MULIB_LINKER_ERANGE && bad == 0);
    mulib_linker_destroy(l);
    l = mulib_linker_create(1, 1); assert(l);
    assert(mulib_linker_define(l, "beyond-image", 9, 0) == 0);
    assert(mulib_linker_relocate(l, "beyond-image", 0, MULIB_LINKER_ABS32, 0) == 0);
    assert(mulib_linker_apply(l, &e, 0, &bad) == MULIB_LINKER_ERANGE && bad == 0);
    mulib_linker_destroy(l);
    assert(!mulib_linker_create(SIZE_MAX, 1));
    assert(!mulib_linker_create(1, SIZE_MAX));
    mulib_linker_destroy(NULL);
}

static void expected(const mulib_dsymspec_t *s, const char *mnemonic,
                     const mulib_assembler_operand_t *ops, size_t n, const char *bytes, size_t length)
{
    unsigned char storage[32];
    mulib_emitter_t e;
    mulib_assembler_error_t error;
    assert(mulib_emitter_init(&e, storage, sizeof(storage), MULIB_EMITTER_LITTLE) == 0);
    assert(mulib_assembler_emit(s, &e, NULL, mnemonic, ops, n, &error) == 0 && !error.message);
    assert(e.size == length && !memcmp(storage, bytes, length));
}

static void assembler(const mulib_dsymspec_t *s)
{
    unsigned char bytes[64] = {0};
    mulib_emitter_t e;
    mulib_assembler_operand_t ops[2];
    mulib_assembler_error_t error;
    mulib_dsymspec_t *other;
    mulib_json_t *clone, *encodings, *entry, *opcode;
    char *text;
    size_t i, symbols;
    mulib_linker_t *l = mulib_linker_create(1, 1); assert(l);
    expected(s, "ret", NULL, 0, "\xc3", 1);
    ops[0] = mulib_assembler_reg("rax"); ops[1] = mulib_assembler_imm(42);
    expected(s, "mov", ops, 2, "\x48\xb8\x2a\0\0\0\0\0\0\0", 10);
    ops[1] = mulib_assembler_reg("rdi");
    expected(s, "mov", ops, 2, "\x48\x89\xf8", 3);
    expected(s, "add", ops, 2, "\x48\x01\xf8", 3);
    expected(s, "imul", ops, 2, "\x48\x0f\xaf\xc7", 4);
    ops[0] = mulib_assembler_reg("r8"); ops[1] = mulib_assembler_reg("r9");
    expected(s, "mov", ops, 2, "\x4d\x89\xc8", 3);
    ops[0] = mulib_assembler_reg("eax"); ops[1] = mulib_assembler_reg("edi");
    expected(s, "mov", ops, 2, "\x89\xf8", 2);
    ops[1] = mulib_assembler_imm(UINT32_MAX);
    expected(s, "mov", ops, 2, "\xb8\xff\xff\xff\xff", 5);
    ops[0] = mulib_assembler_reg("rax"); ops[1] = mulib_assembler_imm(INT64_MIN);
    expected(s, "mov", ops, 2, "\x48\xb8\0\0\0\0\0\0\0\x80", 10);
    ops[0] = mulib_assembler_reg("rax"); ops[1] = mulib_assembler_imm(3);
    expected(s, "add", ops, 2, "\x48\x81\xc0\x03\0\0\0", 7);
    ops[0] = mulib_assembler_reg("r9");
    expected(s, "not", ops, 1, "\x49\xf7\xd1", 3);
    expected(s, "call", ops, 1, "\x41\xff\xd1", 3);
    ops[0] = mulib_assembler_reg("r12");
    expected(s, "push", ops, 1, "\x41\x54", 2);
    expected(s, "pop", ops, 1, "\x41\x5c", 2);
    ops[0] = mulib_assembler_reg("rax"); ops[1] = mulib_assembler_reg("cl");
    expected(s, "shl", ops, 2, "\x48\xd3\xe0", 3);
    ops[0] = mulib_assembler_reg("r9"); ops[1] = mulib_assembler_imm(7);
    expected(s, "shr", ops, 2, "\x49\xc1\xe9\x07", 4);
    assert(mulib_emitter_init(&e, bytes, sizeof(bytes), MULIB_EMITTER_LITTLE) == 0);
    assert(mulib_assembler_emit(s, &e, l, "mov", NULL, 1, &error) == MULIB_ASSEMBLER_EINVAL);
    assert(mulib_assembler_emit(s, &e, l, "mov", ops, 3, &error) == MULIB_ASSEMBLER_EINVAL);
    ops[0] = mulib_assembler_reg("eax"); ops[1] = mulib_assembler_imm(INT64_MAX);
    assert(mulib_assembler_emit(s, &e, l, "mov", ops, 2, &error) == MULIB_ASSEMBLER_ERANGE && error.message && e.size == 0);
    ops[0] = mulib_assembler_reg("rax"); ops[1] = mulib_assembler_imm(INT64_MAX);
    assert(mulib_assembler_emit(s, &e, l, "add", ops, 2, &error) == MULIB_ASSEMBLER_ERANGE);
    ops[1] = mulib_assembler_imm(256);
    assert(mulib_assembler_emit(s, &e, l, "shl", ops, 2, &error) == MULIB_ASSEMBLER_ERANGE);
    ops[0] = mulib_assembler_reg("r16");
    assert(mulib_assembler_emit(s, &e, l, "mov", ops, 2, &error) == MULIB_ASSEMBLER_EREGISTER);
    ops[0] = mulib_assembler_reg("bad");
    assert(mulib_assembler_emit(s, &e, l, "not", ops, 1, &error) == MULIB_ASSEMBLER_EREGISTER);
    ops[0] = mulib_assembler_reg("ax");
    assert(mulib_assembler_emit(s, &e, l, "not", ops, 1, &error) == MULIB_ASSEMBLER_EUNSUPPORTED);
    assert(mulib_assembler_emit(s, &e, l, "vaddps", NULL, 0, &error) == MULIB_ASSEMBLER_EUNSUPPORTED);
    ops[0] = mulib_assembler_symbol("target", INT64_MIN);
    assert(mulib_assembler_emit(s, &e, l, "jmp", ops, 1, &error) == MULIB_ASSEMBLER_ERANGE);
    ops[0] = mulib_assembler_symbol("target", 0);
    assert(mulib_assembler_emit(s, &e, NULL, "jmp", ops, 1, &error) == MULIB_ASSEMBLER_EINVAL);
    assert(mulib_emitter_init(&e, bytes, 4, MULIB_EMITTER_LITTLE) == 0);
    assert(mulib_assembler_emit(s, &e, l, "jmp", ops, 1, &error) == MULIB_ASSEMBLER_ENOSPACE);
    assert(mulib_linker_symbol_count(l) == 0);
    assert(mulib_emitter_init(&e, bytes, sizeof(bytes), MULIB_EMITTER_LITTLE) == 0);
    assert(mulib_assembler_emit(s, &e, l, "jmp", ops, 1, &error) == 0);
    symbols = mulib_linker_symbol_count(l);
    ops[0] = mulib_assembler_symbol("second", 0);
    assert(mulib_assembler_emit(s, &e, l, "jmp", ops, 1, &error) == MULIB_ASSEMBLER_ELINK);
    assert(e.size == 5 && mulib_linker_symbol_count(l) == symbols && error.linker_result == MULIB_LINKER_ELIMIT);
    assert(mulib_linker_define(l, "target", 0, 0) == 0);
    assert(mulib_linker_apply(l, &e, 0, NULL) == 0);
    assert(!memcmp(bytes, "\xe9\xfb\xff\xff\xff", 5));
    mulib_linker_destroy(l);
    other = mulib_dsymspec_load_builtin("aarch64", NULL); assert(other);
    assert(mulib_assembler_emit(other, &e, NULL, "ret", NULL, 0, &error) == MULIB_ASSEMBLER_EUNSUPPORTED);
    mulib_dsymspec_destroy(other);
    /* Changing cloned metadata changes encoded bytes: there is no opcode switch. */
    text = mulib_json_stringify(mulib_dsymspec_document(s), 0); assert(text);
    clone = mulib_json_parse(text, strlen(text), NULL); free(text); assert(clone);
    entry = mulib_json_at(mulib_json_get(clone, "targets"), 0);
    assert(mulib_json_set(entry, "byte_order", mulib_json_string("big")));
    text = mulib_json_stringify(clone, 0); assert(text);
    other = mulib_dsymspec_load_json(text, strlen(text), NULL); assert(other); free(text);
    assert(mulib_assembler_emit(other, &e, NULL, "ret", NULL, 0, &error) == MULIB_ASSEMBLER_EUNSUPPORTED && e.size == 5);
    mulib_dsymspec_destroy(other);
    assert(mulib_json_set(entry, "byte_order", mulib_json_string("le")));
    text = mulib_json_stringify(clone, 0); assert(text);
    other = mulib_dsymspec_load_json(text, strlen(text), NULL); assert(other); free(text);
    expected(other, "ret", NULL, 0, "\xc3", 1);
    mulib_dsymspec_destroy(other);
    encodings = mulib_json_get(mulib_json_get(clone, "_mulib_codegen"), "encodings");
    entry = NULL;
    for (i = 0; i < mulib_json_size(encodings); ++i) {
        mulib_json_t *r = mulib_json_at(encodings, i);
        if (!strcmp(mulib_json_text(mulib_json_get(r, "mnemonic"), NULL), "nop")) { entry = r; break; }
    }
    assert(entry);
    opcode = mulib_json_new(MULIB_JSON_ARRAY); assert(opcode);
    assert(mulib_json_append(opcode, mulib_json_number(0x91)));
    assert(mulib_json_set(entry, "opcode", opcode));
    text = mulib_json_stringify(clone, 0); assert(text);
    other = mulib_dsymspec_load_json(text, strlen(text), NULL); assert(other);
    expected(other, "nop", NULL, 0, "\x91", 1);
    mulib_dsymspec_destroy(other); free(text);
    opcode = mulib_json_new(MULIB_JSON_ARRAY); assert(opcode);
    assert(mulib_json_append(opcode, mulib_json_number(256)));
    assert(mulib_json_set(entry, "opcode", opcode));
    text = mulib_json_stringify(clone, 0); assert(text); invalid_metadata(text); free(text);
    mulib_json_destroy(clone);
}

static int64_t i0(void) { return 10; }
static int64_t i1(int64_t a) { return a + 1; }
static int64_t i2(int64_t a, int64_t b) { return a - b; }
static double f0(void) { return 0.5; }
static double f1(double a) { return a * 2.0; }
static double f2(double a, double b) { return a / b; }
static void dyncall(void)
{
    int64_t i = 99;
    double f = 99.0;
    assert(mulib_dyncall_i64_0(i0, &i) == 0 && i == 10);
    assert(mulib_dyncall_i64_1(i1, 5, &i) == 0 && i == 6);
    assert(mulib_dyncall_i64_2(i2, 9, 4, &i) == 0 && i == 5);
    assert(mulib_dyncall_f64_0(f0, &f) == 0 && f == 0.5);
    assert(mulib_dyncall_f64_1(f1, 0.5, &f) == 0 && f == 1.0);
    assert(mulib_dyncall_f64_2(f2, 3.0, 2.0, &f) == 0 && f == 1.5);
    assert(mulib_dyncall_i64_0(NULL, &i) == -1 && i == 5);
    assert(mulib_dyncall_i64_1(i1, 1, NULL) == -1);
    assert(mulib_dyncall_i64_2(NULL, 1, 2, &i) == -1);
    assert(mulib_dyncall_f64_0(NULL, &f) == -1 && f == 1.5);
    assert(mulib_dyncall_f64_1(f1, 1, NULL) == -1);
    assert(mulib_dyncall_f64_2(NULL, 1, 2, &f) == -1);
}

static void native_path(const mulib_dsymspec_t *spec)
{
    unsigned char bytes[64];
    mulib_emitter_t e;
    mulib_linker_t *l = mulib_linker_create(2, 2);
    mulib_assembler_operand_t ops[2];
    mulib_jit_t *j = NULL;
    int64_t result = -1;
    assert(l);
    assert(mulib_emitter_init(&e, bytes, sizeof(bytes), MULIB_EMITTER_LITTLE) == 0);
    ops[0] = mulib_assembler_symbol("value", 0);
    assert(mulib_assembler_emit(spec, &e, l, "jmp", ops, 1, NULL) == 0);
    ops[0] = mulib_assembler_reg("rax"); ops[1] = mulib_assembler_imm(7);
    assert(mulib_assembler_emit(spec, &e, l, "mov", ops, 2, NULL) == 0);
    assert(mulib_linker_define(l, "value", e.size, 0) == 0);
    ops[1] = mulib_assembler_imm(42);
    assert(mulib_assembler_emit(spec, &e, l, "mov", ops, 2, NULL) == 0);
    assert(mulib_assembler_emit(spec, &e, l, "ret", NULL, 0, NULL) == 0);
    assert(mulib_linker_apply(l, &e, 0, NULL) == 0);
    assert(e.size == 26 && !memcmp(bytes, "\xe9\x0a\0\0\0", 5));
    mulib_linker_destroy(l);
    assert(mulib_jit_create(NULL, e.size) == MULIB_JIT_EINVAL);
    assert(mulib_jit_create(&j, 0) == MULIB_JIT_EINVAL);
    if (!mulib_jit_supported()) {
        assert(mulib_jit_create(&j, e.size) == MULIB_JIT_ENOTSUP && !j);
        return;
    }
    assert(mulib_jit_create(&j, SIZE_MAX) == MULIB_JIT_ERANGE && !j);
    assert(mulib_jit_create(&j, e.size) == 0 && j);
    assert(mulib_jit_create(&j, e.size) == MULIB_JIT_EINVAL);
    assert(mulib_jit_seal(j) == MULIB_JIT_ESTATE);
    assert(mulib_jit_write(j, e.size, NULL, 0) == 0);
    assert(mulib_jit_seal(j) == MULIB_JIT_ESTATE);
    assert(mulib_jit_run_i64_0(j, &result) == MULIB_JIT_ESTATE && result == -1);
    assert(mulib_jit_write(j, SIZE_MAX, bytes, 1) == MULIB_JIT_ERANGE);
    assert(mulib_jit_write(j, 0, NULL, 1) == MULIB_JIT_EINVAL);
    assert(mulib_jit_write(j, 0, bytes, e.size) == 0);
    assert(mulib_jit_seal(j) == 0 && mulib_jit_seal(j) == 0);
    assert(mulib_jit_write(j, 0, bytes, 1) == MULIB_JIT_ESTATE);
    assert(mulib_jit_run_i64_0(j, &result) == 0 && result == 42);
    result = 0;
    assert(mulib_jit_run_i64_0(j, &result) == 0 && result == 42);
    assert(mulib_jit_destroy(j) == 0);
    assert(mulib_jit_destroy(NULL) == 0);
}

int main(void)
{
    mulib_dsymspec_t *spec = metadata();
    emitter(); linker(); assembler(spec); dyncall(); native_path(spec);
    mulib_dsymspec_destroy(spec);
    return 0;
}
