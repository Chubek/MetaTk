#include "MetaArchABI.h"
#include "DSymspec.h"
#include <stdlib.h>
#include <string.h>

struct MaSpec { DsymSpec *impl; };
struct MaDiagnostics { DsymDiags impl; };
struct MaResult {
    DsymBuf data;
    DsymEncoded encoded;
    char *relocation_names[DSYM_ENCODE_MAX_RELOCS];
};

_Static_assert((int)MA_JSON == (int)DSYM_FORMAT_JSON && (int)MA_SEXPR == (int)DSYM_FORMAT_SEXPR,
               "C ABI format values must remain stable");
_Static_assert((int)MA_LOAD_LINT == (int)DSYM_LOAD_LINT && (int)MA_WRITE_SORT_KEYS == (int)DSYM_WRITE_SORT_KEYS,
               "C ABI flag values must remain stable");
_Static_assert((int)MA_AUTO == (int)DSYM_FORMAT_AUTO && (int)MA_YAML == (int)DSYM_FORMAT_YAML &&
               (int)MA_XML == (int)DSYM_FORMAT_XML && (int)MA_NOTE == (int)DSYM_NOTE &&
               (int)MA_WARNING == (int)DSYM_WARNING && (int)MA_ERROR == (int)DSYM_ERROR &&
               (int)MA_LITTLE_ENDIAN == (int)DSYM_ENDIAN_LITTLE &&
               (int)MA_BIG_ENDIAN == (int)DSYM_ENDIAN_BIG, "C ABI enum values must remain stable");
_Static_assert((int)MA_LOAD_BACKEND == (int)DSYM_LOAD_BACKEND &&
               (int)MA_LOAD_NO_COMPILE == (int)DSYM_LOAD_NO_COMPILE &&
               (int)MA_LOAD_STRICT == (int)DSYM_LOAD_STRICT &&
               (int)MA_WRITE_COMPACT == (int)DSYM_WRITE_COMPACT &&
               (int)MA_WRITE_EMBED_BACKEND == (int)DSYM_WRITE_EMBED_BACKEND &&
               (int)MA_WRITE_REF_BACKEND == (int)DSYM_WRITE_REF_BACKEND &&
               (int)MA_WRITE_NO_HEADER == (int)DSYM_WRITE_NO_HEADER,
               "C ABI load/write flags must remain stable");

static void error(MaDiagnostics *d, const char *message) {
    dsym_diag_add(d ? &d->impl : NULL, DSYM_ERROR, NULL, 0, 0, NULL, "%s", message);
}
static bool load_options(MaFormat f, uint32_t flags) {
    return f >= MA_AUTO && f <= MA_SEXPR && !(flags & ~UINT32_C(15));
}

uint32_t metaarch_abi_version(void) { return METAARCH_ABI_VERSION; }
MaDiagnostics *metaarch_diagnostics_create(void) {
    MaDiagnostics *d = malloc(sizeof *d);
    if (d) dsym_diags_init(&d->impl);
    return d;
}
void metaarch_diagnostics_free(MaDiagnostics *d) {
    if (d) { dsym_diags_free(&d->impl); free(d); }
}
void metaarch_diagnostics_clear(MaDiagnostics *d) {
    if (d) { dsym_diags_free(&d->impl); dsym_diags_init(&d->impl); }
}
size_t metaarch_diagnostics_count(const MaDiagnostics *d) { return d ? d->impl.count : 0; }
MaStatus metaarch_diagnostics_get(const MaDiagnostics *d, size_t i, MaDiagnosticView *out) {
    if (!out) return MA_INVALID_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!d) return MA_INVALID_ARGUMENT;
    if (i >= d->impl.count) return MA_NOT_FOUND;
    const DsymDiag *v = &d->impl.items[i];
    *out = (MaDiagnosticView){(int32_t)v->severity, v->line, v->col, v->file, v->path, v->msg};
    return MA_OK;
}

MaSpec *metaarch_spec_load(const char *path, MaFormat f, uint32_t flags, MaDiagnostics *d) {
    if (!path || !load_options(f, flags)) { error(d, "invalid load arguments"); return NULL; }
    MaSpec *s = malloc(sizeof *s);
    if (!s) { error(d, "out of memory"); return NULL; }
    s->impl = dsym_spec_load_file(path, (DsymFormat)f, flags, d ? &d->impl : NULL);
    if (!s->impl) { free(s); return NULL; }
    return s;
}
MaSpec *metaarch_spec_parse(const void *data, size_t len, MaFormat f,
                          const char *path, uint32_t flags, MaDiagnostics *d) {
    if ((!data && len) || !load_options(f, flags)) {
        error(d, "invalid parse arguments"); return NULL;
    }
    MaSpec *s = malloc(sizeof *s);
    if (!s) { error(d, "out of memory"); return NULL; }
    s->impl = dsym_spec_load_buffer(data ? data : "", len, (DsymFormat)f, path, flags,
                                     d ? &d->impl : NULL);
    if (!s->impl) { free(s); return NULL; }
    return s;
}
void metaarch_spec_free(MaSpec *s) {
    if (s) { dsym_spec_free(s->impl); free(s); }
}
int32_t metaarch_spec_load_backend(MaSpec *s, MaDiagnostics *d) {
    if (!s) { error(d, "spec is NULL"); return 0; }
    return dsym_spec_load_backend(s->impl, d ? &d->impl : NULL) ? 1 : 0;
}
size_t metaarch_spec_target_count(const MaSpec *s) { return s ? s->impl->target_count : 0; }
size_t metaarch_spec_register_count(const MaSpec *s) { return s ? s->impl->register_count : 0; }
size_t metaarch_spec_encoder_count(const MaSpec *s) { return s ? s->impl->encoder_count : 0; }
size_t metaarch_spec_form_count(const MaSpec *s) { return s ? s->impl->form_count : 0; }
MaStatus metaarch_spec_target(const MaSpec *s, size_t i, MaTargetView *out) {
    if (!out) return MA_INVALID_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!s) return MA_INVALID_ARGUMENT;
    if (i >= s->impl->target_count) return MA_NOT_FOUND;
    const DsymTarget *v = &s->impl->targets[i];
    *out = (MaTargetView){v->arch, v->subarch, v->obj_format, v->abi, v->addr_size,
                         dsym_target_is_big_endian(v) ? MA_BIG_ENDIAN : MA_LITTLE_ENDIAN};
    return MA_OK;
}
MaStatus metaarch_spec_register(const MaSpec *s, size_t i, MaRegisterView *out) {
    if (!out) return MA_INVALID_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!s) return MA_INVALID_ARGUMENT;
    if (i >= s->impl->register_count) return MA_NOT_FOUND;
    const DsymRegister *v = &s->impl->registers[i];
    *out = (MaRegisterView){v->name, dsym_reg_class_name(v->reg_class), v->width, v->enc};
    return MA_OK;
}
MaStatus metaarch_spec_form(const MaSpec *s, size_t i, MaFormView *out) {
    if (!out) return MA_INVALID_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!s) return MA_INVALID_ARGUMENT;
    if (i >= s->impl->form_count) return MA_NOT_FOUND;
    const DsymForm *v = &s->impl->forms[i];
    *out = (MaFormView){v->mnemonic, v->encoder, v->operand_count, 0};
    for (size_t k = 0; k < v->operand_count; k++)
        if (!v->operands[k].hidden && v->operands[k].kind != DSYM_OK_IMPLICIT)
            out->visible_operand_count++;
    return MA_OK;
}
const char *metaarch_spec_encoder_name(const MaSpec *s, size_t i) {
    return s && i < s->impl->encoder_count ? s->impl->encoders[i].name : NULL;
}
int32_t metaarch_spec_find_register(const MaSpec *s, const char *name) {
    return s && name ? dsym_spec_find_register(s->impl, name) : -1;
}
int32_t metaarch_spec_find_encoder(const MaSpec *s, const char *name) {
    return s && name ? dsym_spec_find_encoder(s->impl, name) : -1;
}

void metaarch_result_free(MaResult *r) {
    if (!r) return;
    for (size_t i = 0; i < DSYM_ENCODE_MAX_RELOCS; i++) free(r->relocation_names[i]);
    dsym_buf_free(&r->data);
    free(r);
}
static MaResult *new_result(MaDiagnostics *d) {
    MaResult *r = calloc(1, sizeof *r);
    if (!r) error(d, "out of memory");
    return r;
}
MaResult *metaarch_spec_write(const MaSpec *s, MaFormat f, uint32_t flags, MaDiagnostics *d) {
    if (!s || f < MA_YAML || f > MA_SEXPR || (flags & ~UINT32_C(31))) {
        error(d, "invalid write arguments"); return NULL;
    }
    MaResult *r = new_result(d);
    if (r && !dsym_spec_write(s->impl, (DsymFormat)f, flags, &r->data, d ? &d->impl : NULL)) {
        error(d, "spec serialization failed");
        metaarch_result_free(r); return NULL;
    }
    return r;
}
static MaResult *finish_encoding(MaResult *r, bool ok, MaDiagnostics *d) {
    if (!ok) { error(d, r->encoded.error); metaarch_result_free(r); return NULL; }
    for (size_t i = 0; i < r->encoded.reloc_count; i++) {
        const char *name = r->encoded.relocs[i].reloc_name;
        size_t n = strlen(name);
        if (n == SIZE_MAX) {
            error(d, "relocation name too long"); metaarch_result_free(r); return NULL;
        }
        r->relocation_names[i] = malloc(n + 1);
        if (!r->relocation_names[i]) {
            error(d, "out of memory"); metaarch_result_free(r); return NULL;
        }
        memcpy(r->relocation_names[i], name, n + 1);
        r->encoded.relocs[i].reloc_name = r->relocation_names[i];
    }
    return r;
}
MaResult *metaarch_spec_encode(const MaSpec *s, const char *encoder, size_t target,
                              const char *const *names, const int64_t *values,
                              size_t n, MaDiagnostics *d) {
    if (!s || !encoder || target >= s->impl->target_count || n > 64 || (n && (!names || !values))) {
        error(d, "invalid encode arguments"); return NULL;
    }
    for (size_t i = 0; i < n; i++) {
        if (!names[i]) { error(d, "operand name is NULL"); return NULL; }
        for (size_t j = 0; j < i; j++)
            if (!strcmp(names[i], names[j])) { error(d, "duplicate operand name"); return NULL; }
    }
    MaResult *r = new_result(d);
    if (!r) return NULL;
    bool ok = dsym_encode_named(s->impl, encoder, target, names, values, n, false, &r->encoded);
    return finish_encoding(r, ok, d);
}
MaResult *metaarch_spec_encode_form(const MaSpec *s, size_t form, size_t target,
                                   const int64_t *values, size_t n, MaDiagnostics *d) {
    if (!s || (n && !values)) { error(d, "invalid form encode arguments"); return NULL; }
    MaResult *r = new_result(d);
    if (!r) return NULL;
    bool ok = dsym_encode_form(s->impl, form, target, values, n, &r->encoded);
    return finish_encoding(r, ok, d);
}
const uint8_t *metaarch_result_data(const MaResult *r) {
    return r ? (r->data.data ? (const uint8_t *)r->data.data : r->encoded.bytes) : NULL;
}
size_t metaarch_result_size(const MaResult *r) {
    return r ? (r->data.data ? r->data.len : r->encoded.size) : 0;
}
size_t metaarch_result_relocation_count(const MaResult *r) { return r ? r->encoded.reloc_count : 0; }
MaStatus metaarch_result_relocation(const MaResult *r, size_t i, MaRelocationView *out) {
    if (!out) return MA_INVALID_ARGUMENT;
    memset(out, 0, sizeof *out);
    if (!r) return MA_INVALID_ARGUMENT;
    if (i >= r->encoded.reloc_count) return MA_NOT_FOUND;
    const DsymEncodedReloc *v = &r->encoded.relocs[i];
    *out = (MaRelocationView){v->reloc_name, v->offset, v->size, v->bit_offset, v->bit_width,
                             (uint32_t)v->endian, v->sym_param, v->sym_value};
    return MA_OK;
}
