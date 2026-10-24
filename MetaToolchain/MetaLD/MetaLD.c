#include "MetaLD.h"
#include "../ArchLD/src/ArchLD.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MetaLD { ArchLD *core; MetaLDError error; int32_t state; int busy; };
static int32_t fail(MetaLDError *e, int32_t code, const char *message) {
    if (e) { e->code = code; snprintf(e->message, sizeof e->message, "%s", message); }
    return code;
}
static int32_t check(MetaLD *ld, int32_t state) {
    if (!ld) return METALD_INVALID;
    if (ld->busy) return fail(&ld->error, METALD_BAD_STATE, "linker callback reentry");
    if (ld->state == METALD_FAILED) return METALD_BAD_STATE; /* Keep original failure. */
    if (ld->state != state) return fail(&ld->error, METALD_BAD_STATE, "operation is invalid in this linker state");
    memset(&ld->error, 0, sizeof ld->error);
    return METALD_OK;
}
static int32_t result(MetaLD *ld, int ok) {
    if (ok) return METALD_OK;
    ld->state = METALD_FAILED;
    const char *message = archld_error(ld->core);
    return fail(&ld->error, METALD_ERROR, *message ? message : "linker operation failed");
}
static int exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f); return 1;
}
uint32_t metald_abi_version(void) { return METALD_ABI_VERSION; }
void metald_options_init(MetaLDOptions *o) {
    if (!o) return;
    memset(o, 0, sizeof *o); o->abi_version = METALD_ABI_VERSION; o->struct_size = sizeof *o;
    o->target = "ELF64"; o->base = 0x401000;
}
MetaLD *metald_create(const MetaLDOptions *o, MetaLDError *error) {
    if (error) memset(error, 0, sizeof *error);
    if (!o || o->abi_version != METALD_ABI_VERSION || o->struct_size < sizeof *o ||
        !o->target || (strcmp(o->target, "ELF64") && strcmp(o->target, "ELF32"))) {
        fail(error, METALD_INVALID, "invalid MetaLD options ABI or target (ELF64/ELF32 required)"); return NULL;
    }
    const char *catalogue = o->catalogue;
    if (!catalogue) catalogue = getenv("METALD_CATALOGUE");
    if (!catalogue || !*catalogue)
        catalogue = exists(METALD_INSTALL_CATALOGUE) ? METALD_INSTALL_CATALOGUE : METALD_SOURCE_CATALOGUE;
    MetaLD *ld = calloc(1, sizeof *ld);
    if (!ld) { fail(error, METALD_NO_MEMORY, "out of memory"); return NULL; }
    ld->core = archld_new(catalogue, o->target, o->base);
    if (!ld->core || *archld_error(ld->core)) {
        fail(error, ld->core ? METALD_ERROR : METALD_NO_MEMORY, ld->core ? archld_error(ld->core) : "out of memory");
        metald_free(ld); return NULL;
    }
    return ld;
}
void metald_free(MetaLD *ld) { if (ld) { archld_free(ld->core); free(ld); } }
const MetaLDError *metald_error(const MetaLD *ld) { return ld ? &ld->error : NULL; }
int32_t metald_state(const MetaLD *ld) { return ld ? ld->state : METALD_FAILED; }
int32_t metald_add_file(MetaLD *ld, const char *path) {
    int32_t rc = check(ld, METALD_COLLECTING); if (rc) return rc;
    if (!path || !*path) return fail(&ld->error, METALD_INVALID, "empty input path");
    return result(ld, archld_add(ld->core, path));
}
int32_t metald_add_memory(MetaLD *ld, const char *name, const void *data, size_t size) {
    int32_t rc = check(ld, METALD_COLLECTING); if (rc) return rc;
    if (!name || !*name || (!data && size)) return fail(&ld->error, METALD_INVALID, "invalid memory input");
    return result(ld, archld_add_memory(ld->core, name, data, size));
}
int32_t metald_define(MetaLD *ld, const char *name, uint64_t address) {
    int32_t rc = check(ld, METALD_COLLECTING); if (rc) return rc;
    if (!name || !*name) return fail(&ld->error, METALD_INVALID, "empty symbol name");
    return result(ld, archld_define_symbol(ld->core, name, address));
}
int32_t metald_set_resolver(MetaLD *ld, MetaLDResolver resolver, void *user) {
    int32_t rc = check(ld, METALD_COLLECTING); if (rc) return rc;
    return result(ld, archld_set_symbol_resolver(ld->core, resolver, user));
}
int32_t metald_link(MetaLD *ld) {
    int32_t rc = check(ld, METALD_COLLECTING); if (rc) return rc;
    ld->busy = 1;
    int ok = archld_link(ld->core);
    ld->busy = 0;
    if (ok) { ld->state = METALD_LINKED; memset(&ld->error, 0, sizeof ld->error); }
    return result(ld, ok);
}
const uint8_t *metald_image(const MetaLD *ld) { return ld && ld->state == METALD_LINKED ? archld_image(ld->core) : NULL; }
size_t metald_image_size(const MetaLD *ld) { return ld && ld->state == METALD_LINKED ? archld_image_size(ld->core) : 0; }
uint64_t metald_image_base(const MetaLD *ld) { return ld ? archld_image_base(ld->core) : 0; }
size_t metald_symbol_count(const MetaLD *ld) { return ld && ld->state == METALD_LINKED ? archld_global_count(ld->core) : 0; }
int32_t metald_symbol_at(const MetaLD *ld, size_t i, MetaLDSymbol *out) {
    if (!ld || !out) return METALD_INVALID;
    memset(out, 0, sizeof *out);
    if (ld->state != METALD_LINKED) return METALD_BAD_STATE;
    return archld_global_at(ld->core, i, &out->name, &out->address, &out->size) ? METALD_OK : METALD_NOT_FOUND;
}
int32_t metald_find_symbol(const MetaLD *ld, const char *name, uint64_t *out) {
    if (!ld || !name || !out) return METALD_INVALID;
    *out = 0;
    if (ld->state != METALD_LINKED) return METALD_BAD_STATE;
    return archld_symbol(ld->core, name, out) ? METALD_OK : METALD_NOT_FOUND;
}
int32_t metald_write_elf(MetaLD *ld, const char *path, uint64_t entry) {
    int32_t rc = check(ld, METALD_LINKED); if (rc) return rc;
    if (!path || !*path) return fail(&ld->error, METALD_INVALID, "empty output path");
    return result(ld, archld_write_elf(ld->core, path, entry));
}
int32_t metald_write_executable(MetaLD *ld, const char *path, const char *entry) {
    int32_t rc = check(ld, METALD_LINKED); if (rc) return rc;
    if (!entry || !*entry) return fail(&ld->error, METALD_INVALID, "empty entry symbol");
    uint64_t address;
    if (!archld_symbol(ld->core, entry, &address)) return fail(&ld->error, METALD_NOT_FOUND, "entry symbol not found");
    return metald_write_elf(ld, path, address);
}
int32_t metald_write_map(MetaLD *ld, const char *path) {
    int32_t rc = check(ld, METALD_LINKED); if (rc) return rc;
    if (!path || !*path) return fail(&ld->error, METALD_INVALID, "empty map path");
    return result(ld, archld_write_map_file(ld->core, path));
}
