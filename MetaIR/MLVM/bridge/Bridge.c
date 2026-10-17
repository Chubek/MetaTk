#include "MLVM/include/MLVM-Bridge.h"
#include "MLVM/bridge/Embedded.h"
#include "qscript/include/QScript-Library.h"
#include "qscript/include/QScript-Runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mlvm_context { qs_context *qs; qs_program *program; char error[4096]; };
struct mlvm_module { qs_value *code; };

static mlvm_status fail(mlvm_context *c, mlvm_status status, const char *message) {
    if (c) snprintf(c->error, sizeof(c->error), "%s", message ? message : "MLVM bridge error");
    return status;
}
static mlvm_status qs_failure(mlvm_context *c) { return fail(c, MLVM_ERROR, qs_error(c->qs)); }
static mlvm_status start(mlvm_context *c) {
    if (!c) return MLVM_INVALID_ARGUMENT;
    c->error[0] = 0;
    return MLVM_OK;
}
static mlvm_status ensure(mlvm_context *c) {
    size_t n, i, length;
    const mlvm_library_source *libraries;
    const unsigned char *entry;
    if (c->program) return MLVM_OK;
    libraries = mlvm_embedded_libraries(&n);
    for (i = 0; i < n; ++i)
        if (qs_add_library(c->qs, libraries[i].name, (const char *)libraries[i].source, libraries[i].length) != QS_OK)
            return qs_failure(c);
    entry = mlvm_embedded_entry(&length);
    c->program = qs_compile(c->qs, (const char *)entry, length);
    return c->program ? MLVM_OK : qs_failure(c);
}
static mlvm_status call(mlvm_context *c, const char *name, const qs_value *const *args, size_t count, qs_value **out) {
    if (ensure(c) != MLVM_OK) return MLVM_ERROR;
    if (qs_call_value(c->qs, c->program, name, args, count, out) != QS_OK) return qs_failure(c);
    return MLVM_OK;
}
static mlvm_status module_result(mlvm_context *c, qs_value *value, mlvm_module **out) {
    mlvm_module *m;
    if (!qs_value_type(value) || strcmp(qs_value_type(value), "list<list<int>>")) {
        qs_value_destroy(value);
        return fail(c, MLVM_ERROR, "MLVM bridge: invalid module result type");
    }
    m = (mlvm_module *)malloc(sizeof(*m));
    if (!m) { qs_value_destroy(value); return fail(c, MLVM_ERROR, "out of memory"); }
    m->code = value; *out = m;
    return MLVM_OK;
}
static mlvm_status parse_bytes(mlvm_context *c, const void *bytes, size_t length, const char *name, mlvm_module **out) {
    qs_value *input, *value = NULL;
    const qs_value *args[1];
    mlvm_status status;
    if (out) *out = NULL;
    if (start(c) != MLVM_OK) return MLVM_INVALID_ARGUMENT;
    if (!out || (!bytes && length)) return fail(c, MLVM_INVALID_ARGUMENT, "null input or output");
    if ((!strcmp(name, "bridge_parse") && length > MLVM_BRIDGE_MAX_SOURCE_BYTES) ||
        (!strcmp(name, "bridge_import") && length > 12u + MLVM_BRIDGE_MAX_INSTRUCTIONS * 12u))
        return fail(c, MLVM_ERROR, "MLVM bridge: input size limit exceeded");
    input = qs_value_string(c->qs, (const char *)bytes, length);
    if (!input) return qs_failure(c);
    args[0] = input;
    status = call(c, name, args, 1, &value);
    qs_value_destroy(input);
    if (status != MLVM_OK) return status;
    return module_result(c, value, out);
}
static mlvm_status string_result(mlvm_context *c, const mlvm_module *m, const char *name, mlvm_buffer *out) {
    qs_value *value = NULL;
    const qs_value *args[1];
    const char *text;
    size_t length;
    mlvm_status status;
    if (out) { out->data = NULL; out->size = 0; }
    if (start(c) != MLVM_OK) return MLVM_INVALID_ARGUMENT;
    if (!m || !out) return fail(c, MLVM_INVALID_ARGUMENT, "null module or output");
    args[0] = m->code;
    status = call(c, name, args, 1, &value);
    if (status != MLVM_OK) return status;
    if (qs_value_get_string(value, &text, &length) != QS_OK || length == SIZE_MAX) {
        qs_value_destroy(value);
        return fail(c, MLVM_ERROR, "MLVM bridge: invalid buffer result");
    }
    out->data = (unsigned char *)malloc(length + 1);
    if (!out->data) { qs_value_destroy(value); return fail(c, MLVM_ERROR, "out of memory"); }
    memcpy(out->data, text, length); out->data[length] = 0; out->size = length;
    qs_value_destroy(value);
    return MLVM_OK;
}

mlvm_context *mlvm_create(void) {
    mlvm_context *c = (mlvm_context *)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->qs = qs_create();
    if (!c->qs) { free(c); return NULL; }
    if (qs_set_stdlib(c->qs, 0) != QS_OK) { qs_destroy(c->qs); free(c); return NULL; }
    return c;
}
void mlvm_destroy(mlvm_context *c) {
    if (c) { qs_program_destroy(c->program); qs_destroy(c->qs); free(c); }
}
const char *mlvm_error(const mlvm_context *c) { return c ? c->error : "null MLVM context"; }
mlvm_status mlvm_set_engine(mlvm_context *c, int engine) {
    if (start(c) != MLVM_OK) return MLVM_INVALID_ARGUMENT;
    if (engine != MLVM_INTERPRETER && engine != MLVM_JIT) return fail(c, MLVM_INVALID_ARGUMENT, "invalid MLVM engine");
    return qs_set_engine(c->qs, engine) == QS_OK ? MLVM_OK : qs_failure(c);
}
mlvm_status mlvm_set_host_step_limit(mlvm_context *c, size_t steps) {
    if (start(c) != MLVM_OK) return MLVM_INVALID_ARGUMENT;
    if (!steps) return fail(c, MLVM_INVALID_ARGUMENT, "host step limit must be positive");
    qs_set_step_limit(c->qs, steps); return MLVM_OK;
}
mlvm_status mlvm_parse(mlvm_context *c, const char *text, size_t length, mlvm_module **out) {
    return parse_bytes(c, text, length, "bridge_parse", out);
}
mlvm_status mlvm_import_mcil(mlvm_context *c, const unsigned char *bytes, size_t length, mlvm_module **out) {
    return parse_bytes(c, bytes, length, "bridge_import", out);
}
mlvm_status mlvm_export_mcil(mlvm_context *c, const mlvm_module *m, mlvm_buffer *out) {
    return string_result(c, m, "bridge_export", out);
}
mlvm_status mlvm_analyze(mlvm_context *c, const mlvm_module *m, mlvm_buffer *out) {
    return string_result(c, m, "bridge_analyze", out);
}
mlvm_status mlvm_text(mlvm_context *c, const mlvm_module *m, mlvm_buffer *out) {
    return string_result(c, m, "bridge_text", out);
}
mlvm_status mlvm_optimize(mlvm_context *c, const mlvm_module *m, int mode, mlvm_module **out) {
    qs_value *option, *value = NULL;
    const qs_value *args[2];
    mlvm_status status;
    if (out) *out = NULL;
    if (start(c) != MLVM_OK) return MLVM_INVALID_ARGUMENT;
    if (!m || !out || mode < MLVM_OPT_NONE || mode > MLVM_OPT_GLOBAL)
        return fail(c, MLVM_INVALID_ARGUMENT, "invalid module, output or optimization mode");
    option = qs_value_int(c->qs, mode);
    if (!option) return qs_failure(c);
    args[0] = m->code; args[1] = option;
    status = call(c, "bridge_optimize", args, 2, &value);
    qs_value_destroy(option);
    if (status != MLVM_OK) return status;
    return module_result(c, value, out);
}
mlvm_status mlvm_run(mlvm_context *c, const mlvm_module *m, int64_t steps, int64_t *out) {
    qs_value *limit, *value = NULL;
    const qs_value *args[2];
    mlvm_status status;
    if (out) *out = 0;
    if (start(c) != MLVM_OK) return MLVM_INVALID_ARGUMENT;
    if (!m || !out || steps <= 0) return fail(c, MLVM_INVALID_ARGUMENT, "invalid module, result or step limit");
    limit = qs_value_int(c->qs, steps);
    if (!limit) return qs_failure(c);
    args[0] = m->code; args[1] = limit;
    status = call(c, "bridge_run", args, 2, &value);
    qs_value_destroy(limit);
    if (status != MLVM_OK) return status;
    if (qs_value_get_int(value, out) != QS_OK) status = fail(c, MLVM_ERROR, "MLVM bridge: invalid integer result");
    qs_value_destroy(value);
    return status;
}
size_t mlvm_module_instruction_count(const mlvm_module *m) { return m ? qs_value_list_size(m->code) : 0; }
void mlvm_module_destroy(mlvm_module *m) { if (m) { qs_value_destroy(m->code); free(m); } }
void mlvm_buffer_destroy(mlvm_buffer *buffer) {
    if (buffer) { free(buffer->data); buffer->data = NULL; buffer->size = 0; }
}
