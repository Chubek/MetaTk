/* Build with libFuzzer; run from ArchLD/ so the descriptions are available.
 * Input objects are always read by ArchBFD, never by a test-side ELF reader. */
#include "ArchLD.h"
#include "ArchBFD.h"
#include <stdlib.h>

static ArchBFD *db;
static const ArchBFDSpec *spec32, *spec64;

int LLVMFuzzerInitialize(int *argc, char ***argv) {
    (void)argc;
    (void)argv;
    DsymDiags d;
    dsym_diags_init(&d);
    db = archbfd_open("targets/ArchBFDCatalogue.json", &d);
    if (db) {
        spec32 = archbfd_load_name(db, "ELF32", 0, &d);
        spec64 = archbfd_load_name(db, "ELF64", 0, &d);
    }
    if (!spec32 || !spec64 || d.errors) abort();
    dsym_diags_free(&d);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    bool is32 = size > 4 && data[4] == 1;
#ifdef FUZZ_LINK
    ArchLD *ld = archld_new("targets/ArchBFDCatalogue.json",
                           is32 ? "ELF32" : "ELF64", 0x401000);
    if (!ld) abort();
    if (archld_add_memory(ld, "fuzz-input", data, size)) {
        (void)archld_link(ld);
        for (size_t i = 0; i < archld_global_count(ld); ++i) {
            const char *name;
            uint64_t address, again;
            if (!archld_global_at(ld, i, &name, &address, NULL) ||
                !archld_symbol(ld, name, &again) || again != address) abort();
        }
    }
    archld_free(ld);
#else
    DsymDiags d;
    dsym_diags_init(&d);
    ArchBFDObject *obj = archbfd_object_open_memory(data, size,
                                                  is32 ? spec32 : spec64, &d);
    archbfd_object_close(obj);
    dsym_diags_free(&d);
#endif
    return 0;
}
