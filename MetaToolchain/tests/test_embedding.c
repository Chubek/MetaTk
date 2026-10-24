#include "MetaAS/MetaAS.h"
#include "MetaLD/MetaLD.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef EMBED_LUA
#define EMBED_LUA 0
#endif
#ifndef EMBED_PLUGIN
#define EMBED_PLUGIN ""
#endif

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
static int directive(void *user, const ArchASInvocation *inv, const ArchASEmitter *out) {
    unsigned *called = user;
    ++*called;
    if (strcmp(inv->target, "amd64") || inv->line != 2 || strcmp(inv->file, "callback.mair")) return -1;
    return out->emit(out->context, ".byte 9,", 8) || out->emit(out->context, inv->arguments, inv->arguments_length);
}
static int oversized(void *user, const ArchASInvocation *inv, const ArchASEmitter *out) {
    (void)user; (void)inv;
    out->emit(out->context, "", SIZE_MAX); /* Host must reject before reading. */
    return 0; /* Ignoring emitter failure must not hide the error. */
}
static int empty_error(void *user, const ArchASInvocation *inv, const ArchASEmitter *out) {
    (void)user; (void)inv;
    out->error(out->context, "");
    return 0; /* Reporting an error must fail even with an empty message. */
}
static int resolve(void *user, const char *name, uint64_t *address, char *message, size_t size) {
    (void)message; (void)size;
    ++*(unsigned *)user;
    if (strcmp(name, "host_pointer")) return 0;
    *address = UINT64_C(0x123456789abcdef0); return 1;
}
static uint64_t little(const uint8_t *p) {
    uint64_t v = 0; for (unsigned i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8 * i); return v;
}
int main(int argc, char **argv) {
    CHECK(argc == 3);
    char database[4096], catalogue[4096], path[4096];
    snprintf(database, sizeof database, "%s/MetaArchDB", argv[1]);
    snprintf(catalogue, sizeof catalogue, "%s/ArchBFDCatalogue.yaml", argv[1]);
    CHECK(metaas_abi_version() == 1 && metald_abi_version() == 1);
    MetaASOptions asopts; metaas_options_init(&asopts);
    asopts.target = "amd64"; asopts.database = database; asopts.backend = METAAS_BUILTIN; asopts.format = METAAS_HEX;
    MetaASError error;
    CHECK(!metaas_create(NULL, &error) && error.code == METAAS_INVALID);
    asopts.abi_version = 99;
    CHECK(!metaas_create(&asopts, &error) && error.code == METAAS_INVALID);
    asopts.abi_version = METAAS_ABI_VERSION;
    MetaAS *as = metaas_create(&asopts, &error);
    CHECK(as && error.code == METAAS_OK);
    unsigned called = 0;
    CHECK(metaas_register_directive(as, "custom", directive, &called) == METAAS_OK);
    CHECK(metaas_register_directive(as, ".custom", directive, &called) != METAAS_OK);
    CHECK(metaas_register_directive(as, "byte", directive, &called) != METAAS_OK);
    CHECK(metaas_define(as, "VALUE", 3) == METAAS_OK);
    CHECK(metaas_add_include(as, argv[2]) == METAAS_OK);
    const char *include = ".include \"value.inc\"\n";
    MetaASResult *inc = metaas_compile(as, include, strlen(include), NULL);
    CHECK(inc && metaas_result_size(inc) == 3 && !memcmp(metaas_result_data(inc), "03\n", 3));
    CHECK(metaas_result_dependency_count(inc) == 1 && strstr(metaas_result_dependency(inc, 0), "value.inc"));
    metaas_result_free(inc);
    const char *source = "# comment\n.custom VALUE\n";
    MetaASResult *r = metaas_compile(as, source, strlen(source), "callback.mair");
    CHECK(r && called == 1 && metaas_result_size(r) == 5 && !memcmp(metaas_result_data(r), "0903\n", 5));
    CHECK(metaas_result_diagnostics(r) && !*metaas_result_diagnostics(r));
    CHECK(!metaas_result_dependency(r, SIZE_MAX));
    metaas_result_free(r);
    CHECK(!metaas_compile(as, NULL, 1, NULL) && metaas_error(as)->code == METAAS_INVALID);
    source = "\nencode nonexistent";
    CHECK(!metaas_compile(as, source, strlen(source), "bad.mair"));
    CHECK(metaas_error(as)->line == 2 && !strcmp(metaas_error(as)->file, "bad.mair"));
    CHECK(metaas_register_directive(as, "oversized", oversized, NULL) == METAAS_OK);
    CHECK(!metaas_compile(as, ".oversized", 10, NULL));
    CHECK(strstr(metaas_error(as)->message, "oversized"));
    CHECK(metaas_register_directive(as, "empty_error", empty_error, NULL) == METAAS_OK);
    CHECK(!metaas_compile(as, ".empty_error", 12, NULL));
    CHECK(strstr(metaas_error(as)->message, "directive failed"));
    r = metaas_compile(as, "encode ret", 10, NULL);
    CHECK(r && metaas_error(as)->code == METAAS_OK);
    metaas_free(as);
    CHECK(metaas_result_size(r) == 3 && !memcmp(metaas_result_data(r), "c3\n", 3));
    metaas_result_free(r);

    /* Optional extension configuration reaches the same compile pipeline. */
    as = metaas_create(&asopts, &error);
    CHECK(as);
    if (EMBED_LUA) {
        snprintf(path, sizeof path, "%s/ArchAS/examples/directives.lua", argv[1]);
        CHECK(metaas_add_lua(as, path) == METAAS_OK);
        r = metaas_compile(as, ".u32 0x12345678", 15, NULL);
        CHECK(r && metaas_result_size(r) == 9 && !memcmp(metaas_result_data(r), "78563412\n", 9));
        metaas_result_free(r);
    }
    if (*EMBED_PLUGIN) {
        CHECK(metaas_add_plugin(as, EMBED_PLUGIN) == METAAS_OK);
        r = metaas_compile(as, ".pair 13", 8, NULL);
        CHECK(r && metaas_result_size(r) == 5 && !memcmp(metaas_result_data(r), "0d0d\n", 5));
        metaas_result_free(r);
    }
    metaas_free(as);

    MetaLDOptions ldopts; metald_options_init(&ldopts); ldopts.catalogue = catalogue;
    MetaLDError lderr;
    CHECK(!metald_create(NULL, &lderr) && lderr.code == METALD_INVALID);
    for (int bits = 64; bits >= 32; bits -= 32) {
        asopts.target = bits == 64 ? "amd64" : "x86";
        asopts.backend = METAAS_GAS; asopts.format = METAAS_OBJECT;
        as = metaas_create(&asopts, &error);
        CHECK(as);
        CHECK(metaas_add_assembler_argument(as, "--fatal-warnings") == METAAS_OK);
        source = bits == 64 ?
            ".text\n.globl _start\n_start: mov $exit_code,%edi; mov $60,%eax; syscall\n.data\n.globl pointers\npointers: .quad host_pointer,host_pointer\n" :
            ".text\n.globl _start\n_start: mov $exit_code,%ebx; mov $1,%eax; int $0x80\n";
        r = metaas_compile(as, source, strlen(source), "embedded.mair");
        if (!r) fprintf(stderr, "%s\n", metaas_error(as)->message);
        CHECK(r && metaas_result_size(r) > 64);
        metaas_free(as);
        ldopts.target = bits == 64 ? "ELF64" : "ELF32";
        MetaLD *ld = metald_create(&ldopts, &lderr);
        CHECK(ld && metald_state(ld) == METALD_COLLECTING);
        CHECK(!metald_image(ld) && !metald_image_size(ld));
        CHECK(metald_add_memory(ld, "bad", NULL, 1) == METALD_INVALID);
        CHECK(metald_state(ld) == METALD_COLLECTING);
        CHECK(metald_add_memory(ld, "copied.o", metaas_result_data(r), metaas_result_size(r)) == METALD_OK);
        metaas_result_free(r); /* Linker owns its input. */
        CHECK(metald_define(ld, "exit_code", 37) == METALD_OK);
        unsigned resolved = 0;
        CHECK(metald_set_resolver(ld, resolve, &resolved) == METALD_OK);
        CHECK(metald_link(ld) == METALD_OK);
        CHECK(resolved == (bits == 64 ? 1u : 0u));
        CHECK(metald_state(ld) == METALD_LINKED && metald_image(ld));
        uint64_t address;
        CHECK(metald_find_symbol(ld, "exit_code", &address) == METALD_OK && address == 37);
        CHECK(metald_find_symbol(ld, "absent", &address) == METALD_NOT_FOUND && address == 0);
        if (bits == 64) {
            CHECK(metald_find_symbol(ld, "pointers", &address) == METALD_OK);
            size_t offset = (size_t)(address - metald_image_base(ld));
            CHECK(offset + 16 <= metald_image_size(ld));
            CHECK(little(metald_image(ld) + offset) == UINT64_C(0x123456789abcdef0));
            CHECK(little(metald_image(ld) + offset + 8) == UINT64_C(0x123456789abcdef0));
        }
        MetaLDSymbol symbol;
        for (size_t i = 0; i < metald_symbol_count(ld); ++i) {
            CHECK(metald_symbol_at(ld, i, &symbol) == METALD_OK);
            CHECK(metald_find_symbol(ld, symbol.name, &address) == METALD_OK && symbol.address == address);
        }
        CHECK(metald_symbol_at(ld, SIZE_MAX, &symbol) == METALD_NOT_FOUND);
        CHECK(metald_link(ld) == METALD_BAD_STATE);
        CHECK(metald_add_memory(ld, "late", "", 0) == METALD_BAD_STATE);
        CHECK(metald_write_executable(ld, "ignored", "absent") == METALD_NOT_FOUND);
        snprintf(path, sizeof path, "%s/c-linked%d", argv[2], bits);
        CHECK(metald_write_executable(ld, path, "_start") == METALD_OK);
        snprintf(path, sizeof path, "%s/c-linked%d.map", argv[2], bits);
        CHECK(metald_write_map(ld, path) == METALD_OK);
        metald_free(ld);
    }
    MetaLD *ld = metald_create(&ldopts, &lderr);
    CHECK(ld && metald_define(ld, "wide", UINT64_MAX) == METALD_ERROR);
    CHECK(metald_state(ld) == METALD_FAILED && strstr(metald_error(ld)->message, "ELF32"));
    metald_free(ld);
    ld = metald_create(&ldopts, &lderr);
    CHECK(ld && metald_add_memory(ld, "invalid.o", "bad", 3) == METALD_ERROR);
    CHECK(metald_state(ld) == METALD_FAILED && metald_link(ld) == METALD_BAD_STATE);
    metald_free(ld);
    metaas_free(NULL); metaas_result_free(NULL); metald_free(NULL);
    CHECK(!metaas_error(NULL) && !metald_error(NULL) && !metaas_result_data(NULL));
    printf("%u C embedding checks passed\n", checks);
    return 0;
}
