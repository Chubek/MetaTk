#ifndef MLVM_BRIDGE_H
#define MLVM_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MLVM_BRIDGE_ABI 1
#define MLVM_BRIDGE_MAX_INSTRUCTIONS 4096u
#define MLVM_BRIDGE_MAX_SOURCE_BYTES (1024u * 1024u)
typedef struct mlvm_context mlvm_context;
typedef struct mlvm_module mlvm_module;
typedef enum mlvm_status { MLVM_OK = 0, MLVM_ERROR = 1, MLVM_INVALID_ARGUMENT = 2 } mlvm_status;
typedef enum mlvm_engine { MLVM_INTERPRETER = 0, MLVM_JIT = 1 } mlvm_engine;
typedef enum mlvm_optimization { MLVM_OPT_NONE = 0, MLVM_OPT_LOCAL = 1, MLVM_OPT_GLOBAL = 2 } mlvm_optimization;
typedef struct mlvm_buffer { unsigned char *data; size_t size; } mlvm_buffer;

/* Contexts own an embedded QScript program; no library paths or source files
 * are needed at runtime. NULL from create means allocation/setup failure. Other
 * failures are reported by operations through status and mlvm_error(). */
mlvm_context *mlvm_create(void);
void mlvm_destroy(mlvm_context *);
/* Borrowed until the next operation on this context; at most 4095 bytes. */
const char *mlvm_error(const mlvm_context *);
mlvm_status mlvm_set_engine(mlvm_context *, int engine);
mlvm_status mlvm_set_host_step_limit(mlvm_context *, size_t steps);

/* Output parameters are cleared on entry, including on errors. Do not pass
 * an output slot holding an unreleased module/buffer. Text is length-delimited;
 * buffers additionally carry a trailing NUL not included in size. Input bytes
 * are copied, never retained. Contexts must not be used concurrently/reentered. */
mlvm_status mlvm_parse(mlvm_context *, const char *text, size_t length, mlvm_module **);
mlvm_status mlvm_import_mcil(mlvm_context *, const unsigned char *bytes, size_t length, mlvm_module **);
mlvm_status mlvm_export_mcil(mlvm_context *, const mlvm_module *, mlvm_buffer *);
mlvm_status mlvm_optimize(mlvm_context *, const mlvm_module *, int mode, mlvm_module **);
mlvm_status mlvm_run(mlvm_context *, const mlvm_module *, int64_t max_steps, int64_t *result);
mlvm_status mlvm_analyze(mlvm_context *, const mlvm_module *, mlvm_buffer *json);
mlvm_status mlvm_text(mlvm_context *, const mlvm_module *, mlvm_buffer *text);

/* Immutable owning modules/buffers outlive their creation context and can be
 * consumed by another context. Destroy/free each owned result exactly once. */
size_t mlvm_module_instruction_count(const mlvm_module *);
void mlvm_module_destroy(mlvm_module *);
void mlvm_buffer_destroy(mlvm_buffer *);

#ifdef __cplusplus
}
#endif
#endif
