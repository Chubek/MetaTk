/*
 * ArchDB.h — catalogue of architecture specifications.
 *
 * An ArchDB is opened from either
 *   - a catalogue file (ArchDBCatalogue.yaml, any supported format), or
 *   - a directory, which is scanned for *.dsymspec.{yaml,yml,json,xml,sexp}.
 *
 * Specs are loaded lazily on first use and cached until archdb_close().
 *
 * Catalogue format (schema 1):
 *
 *   schema: 1
 *   root: ArchDB                 # directory of spec files, relative to catalogue
 *   architectures:
 *     - {name: x86, file: x86.dsymspec.yaml, arch: x86, subarch: i386,
 *        addr_size: 32, byte_order: little, obj_format: ELF32,
 *        backend: backend/x86.json}
 *
 * Only `name` and `file` are required; the remaining keys are summary data
 * so callers can choose an architecture without parsing every spec.
 */
#ifndef METAARCH_ARCHDB_H
#define METAARCH_ARCHDB_H

#include "DSymspec.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ARCHDB_CATALOGUE_SCHEMA 1

typedef struct ArchDBEntry {
    const char *name;    /* catalogue key, e.g. "amd64" */
    const char *file;    /* path relative to root */
    const char *path;    /* resolved path */
    const char *arch;    /* targets[0].arch (summary; may be NULL) */
    const char *subarch;
    int64_t addr_size;   /* 0 if unknown */
    const char *byte_order;
    const char *obj_format;
    const char *backend; /* backend file relative to root, or NULL */
} ArchDBEntry;

typedef struct ArchDB ArchDB;

ArchDB *archdb_open(const char *path, DsymDiags *diags);
/* Build a catalogue by loading every spec found in `dir` (fills summaries). */
ArchDB *archdb_scan(const char *dir, DsymDiags *diags);
void archdb_close(ArchDB *db);

size_t archdb_count(const ArchDB *db);
const ArchDBEntry *archdb_entry(const ArchDB *db, size_t i);
/* Lookup by catalogue name, then by targets[].arch, then subarch. */
const ArchDBEntry *archdb_find(const ArchDB *db, const char *name);

/* Lazily load (and cache) a spec. `flags` are DSYM_LOAD_* and are only
   honoured on the first load of each entry. */
const DsymSpec *archdb_load(ArchDB *db, const ArchDBEntry *e, uint32_t flags,
                            DsymDiags *diags);
const DsymSpec *archdb_load_name(ArchDB *db, const char *name, uint32_t flags,
                                 DsymDiags *diags);

/* Serialise the catalogue. `root` is written as given (NULL -> the db root
   relative to nothing, i.e. as opened). */
bool archdb_write_catalogue(const ArchDB *db, DsymFormat fmt, const char *root,
                            DsymBuf *out, DsymDiags *diags);

#ifdef __cplusplus
}
#endif
#endif /* METAARCH_ARCHDB_H */
