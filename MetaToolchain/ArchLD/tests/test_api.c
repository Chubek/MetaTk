#include "ArchLD.h"
#include "ArchBFD.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static void *inspect(void *argument) {
    const ArchLD *ld = argument;
    for (unsigned repeat = 0; repeat < 1000; ++repeat) {
        for (size_t i = 0; i < archld_global_count(ld); ++i) {
            const char *name;
            uint64_t address, size, again;
            assert(archld_global_at(ld, i, &name, &address, &size));
            assert(archld_symbol(ld, name, &again));
            assert(address == again);
        }
        assert(archld_image(ld) && archld_image_size(ld));
        assert(!*archld_error(ld));
    }
    return NULL;
}

int main(int argc, char **argv) {
    assert(argc == 5);
    const char *catalogue = argv[1], *target = argv[2];
    DsymBuf bytes;
    dsym_buf_init(&bytes);
    assert(dsym_buf_read_file(&bytes, argv[3]));
    DsymDiags diagnostics;
    dsym_diags_init(&diagnostics);
    ArchBFD *db = archbfd_open(catalogue, &diagnostics);
    assert(db);
    const ArchBFDSpec *spec = archbfd_load_name(db, target, 0, &diagnostics);
    assert(spec);
    ArchBFDObject *object = archbfd_object_open_memory(bytes.data, bytes.len, spec, &diagnostics);
    assert(object && !diagnostics.errors);
    assert(archbfd_object_reloc_count(object));
    for (size_t i = 0; i < archbfd_object_reloc_count(object); ++i) {
        const ArchBFDObjectReloc *r = archbfd_object_reloc(object, i);
        const ArchBFDObjectSection *source = archbfd_object_section(object, r->source_section);
        assert(source && (source->type == 4 || source->type == 9));
        assert(r->has_addend == (source->type == 4));
        assert(r->target_section == (size_t)source->info);
        assert(archbfd_object_section(object, r->target_section));
    }
    archbfd_object_close(object);
    archbfd_close(db);
    dsym_diags_free(&diagnostics);

    ArchLD *ld = archld_new(catalogue, target, 0x401000);
    assert(ld && !*archld_error(ld));
    assert(!archld_image(ld) && !archld_global_count(ld));
    assert(!archld_write_map_file(ld, NULL));
    assert(archld_add_memory(ld, "copied-object", bytes.data, bytes.len));
    memset(bytes.data, 0xff, bytes.len); /* ArchLD owns an independent copy. */
    dsym_buf_free(&bytes);
    assert(archld_add(ld, argv[4]));
    assert(archld_link(ld));
    uint64_t entry;
    assert(archld_symbol(ld, "_start", &entry));
    assert(!archld_write_elf(ld, NULL, entry));
    assert(!archld_write_elf(ld, "", entry));
    assert(!archld_symbol(ld, "missing", &entry));
    assert(!archld_global_at(ld, archld_global_count(ld), NULL, NULL, NULL));
    pthread_t threads[4];
    for (size_t i = 0; i < 4; ++i) assert(!pthread_create(&threads[i], NULL, inspect, ld));
    for (size_t i = 0; i < 4; ++i) assert(!pthread_join(threads[i], NULL));
    assert(!archld_add_memory(ld, "late", "", 0));
    archld_free(ld);

    ld = archld_new(catalogue, target, 0x401000);
    assert(ld && !archld_add_memory(ld, "invalid", NULL, 1));
    assert(strstr(archld_error(ld), "invalid memory input"));
    assert(!archld_add(ld, argv[3])); /* Sticky failure. */
    archld_free(ld);
    archld_free(NULL);
    return 0;
}
