// dynload.h - Tourchstone dynamic-loader and object-inspection API.
//
// The C layer is intentionally small and stable: native loading uses the host
// loader (dlopen/LoadLibrary), while object metadata comes from Yellowstone's
// qBFD readers. Strings returned from library/query functions are borrowed and
// remain valid until the owning handle is closed/destroyed.
#ifndef TOURCHSTONE_DYNLOAD_H
#define TOURCHSTONE_DYNLOAD_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(TDYNLOAD_SHARED)
#  ifdef tdynload_EXPORTS
#    define TDYN_API __declspec(dllexport)
#  else
#    define TDYN_API __declspec(dllimport)
#  endif
#else
#  define TDYN_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdyn_library tdyn_library_t;
typedef struct tdyn_env tdyn_env_t;

typedef enum tdyn_status {
  TDYN_OK = 0,
  TDYN_ERROR = 1,
  TDYN_INVALID_ARGUMENT = 2,
  TDYN_NOT_FOUND = 3,
  TDYN_NATIVE_ERROR = 4,
  TDYN_INSPECT_ERROR = 5,
  TDYN_UNSUPPORTED = 6,
  TDYN_OUT_OF_MEMORY = 7,
  TDYN_BUFFER_TOO_SMALL = 8
} tdyn_status_t;

typedef enum tdyn_open_flag {
  TDYN_OPEN_LAZY = 1u << 0,
  TDYN_OPEN_NOW = 1u << 1,
  TDYN_OPEN_LOCAL = 1u << 2,
  TDYN_OPEN_GLOBAL = 1u << 3,
  TDYN_OPEN_NODELETE = 1u << 4,
  TDYN_OPEN_NOLOAD = 1u << 5,
  TDYN_OPEN_DEEPBIND = 1u << 6,
  TDYN_OPEN_REQUIRE_INSPECT = 1u << 7,
  TDYN_OPEN_INSPECT_ONLY = 1u << 8
} tdyn_open_flag_t;

typedef enum tdyn_symbol_table {
  TDYN_SYMBOL_TABLE_STATIC = 0,
  TDYN_SYMBOL_TABLE_DYNAMIC = 1,
  TDYN_SYMBOL_TABLE_ANY = 2
} tdyn_symbol_table_t;

typedef enum tdyn_symbol_binding {
  TDYN_SYMBOL_LOCAL = 0,
  TDYN_SYMBOL_GLOBAL = 1,
  TDYN_SYMBOL_WEAK = 2,
  TDYN_SYMBOL_UNIQUE = 3
} tdyn_symbol_binding_t;

typedef enum tdyn_symbol_kind {
  TDYN_SYMBOL_NONE = 0,
  TDYN_SYMBOL_OBJECT = 1,
  TDYN_SYMBOL_FUNCTION = 2,
  TDYN_SYMBOL_SECTION = 3,
  TDYN_SYMBOL_FILE = 4,
  TDYN_SYMBOL_COMMON = 5,
  TDYN_SYMBOL_TLS = 6,
  TDYN_SYMBOL_INDIRECT = 7
} tdyn_symbol_kind_t;

typedef struct tdyn_symbol_info {
  const char* name;
  uint64_t value;
  uint64_t size;
  uint32_t flags;
  uint32_t native_type;
  uint32_t section;
  int has_section;
  tdyn_symbol_binding_t binding;
  tdyn_symbol_kind_t kind;
  tdyn_symbol_table_t table;
  int undefined;
  int exported;
  int imported;
  int hidden;
} tdyn_symbol_info_t;

typedef struct tdyn_section_info {
  const char* name;
  uint32_t index;
  uint64_t vma;
  uint64_t lma;
  uint64_t size;
  uint64_t file_offset;
  uint64_t file_size;
  uint64_t alignment;
  uint32_t flags;
  uint64_t native_flags;
  uint32_t native_type;
} tdyn_section_info_t;

typedef struct tdyn_relocation_info {
  uint64_t offset;
  int64_t addend;
  uint32_t native_type;
  const char* type_name;
  uint8_t bit_size;
  int pc_relative;
  int has_symbol;
  const char* symbol_name;
  uint32_t symbol_index;
  tdyn_symbol_table_t symbol_table;
  int has_target_section;
  uint32_t target_section;
} tdyn_relocation_info_t;

TDYN_API const char* tdyn_version(void);
TDYN_API const char* tdyn_status_string(tdyn_status_t status);
TDYN_API const char* tdyn_last_error(void);

TDYN_API const char* tdyn_symbol_binding_name(tdyn_symbol_binding_t binding);
TDYN_API const char* tdyn_symbol_kind_name(tdyn_symbol_kind_t kind);

TDYN_API tdyn_status_t tdyn_env_create(tdyn_env_t** out);
// Pointer-return convenience; NULL on failure, diagnostic via tdyn_last_error.
TDYN_API tdyn_env_t* tdyn_env_new(void);
TDYN_API void tdyn_env_destroy(tdyn_env_t* env);
TDYN_API tdyn_status_t tdyn_env_add_search_path(tdyn_env_t* env,
                                                const char* path);
TDYN_API tdyn_status_t tdyn_env_add_default_search_paths(tdyn_env_t* env);
TDYN_API size_t tdyn_env_search_path_count(const tdyn_env_t* env);
TDYN_API const char* tdyn_env_search_path(const tdyn_env_t* env, size_t index);
TDYN_API tdyn_status_t tdyn_env_resolve_library(tdyn_env_t* env,
                                                const char* name,
                                                char* out,
                                                size_t* inout_size);

TDYN_API tdyn_status_t tdyn_library_open(const char* path,
                                         uint32_t flags,
                                         tdyn_library_t** out);
TDYN_API tdyn_library_t* tdyn_library_load(const char* path, uint32_t flags);
TDYN_API tdyn_status_t tdyn_library_open_self(uint32_t flags,
                                              tdyn_library_t** out);
// Closing an environment-owned handle removes it and its promoted symbols.
// All pointers into that library are invalid after close (including aliases).
TDYN_API void tdyn_library_close(tdyn_library_t* library);

TDYN_API tdyn_status_t tdyn_env_open(tdyn_env_t* env,
                                     const char* path,
                                     uint32_t flags,
                                     tdyn_library_t** out);
TDYN_API tdyn_status_t tdyn_env_open_self(tdyn_env_t* env,
                                          uint32_t flags,
                                          tdyn_library_t** out);
TDYN_API size_t tdyn_env_library_count(const tdyn_env_t* env);
TDYN_API tdyn_library_t* tdyn_env_library(tdyn_env_t* env, size_t index);

TDYN_API const char* tdyn_library_path(const tdyn_library_t* library);
TDYN_API int tdyn_library_is_native_loaded(const tdyn_library_t* library);
TDYN_API int tdyn_library_is_inspected(const tdyn_library_t* library);
TDYN_API const char* tdyn_library_inspection_error(
    const tdyn_library_t* library);
TDYN_API const char* tdyn_library_format_name(const tdyn_library_t* library);
TDYN_API const char* tdyn_library_arch_name(const tdyn_library_t* library);
TDYN_API const char* tdyn_library_endian_name(const tdyn_library_t* library);
TDYN_API const char* tdyn_library_file_type_name(const tdyn_library_t* library);
TDYN_API uint64_t tdyn_library_entry_point(const tdyn_library_t* library);
TDYN_API uint64_t tdyn_library_image_base(const tdyn_library_t* library);

TDYN_API tdyn_status_t tdyn_library_symbol(tdyn_library_t* library,
                                           const char* name,
                                           void** out);
TDYN_API tdyn_status_t tdyn_library_find_symbol_info(
    const tdyn_library_t* library,
    const char* name,
    tdyn_symbol_table_t table,
    tdyn_symbol_info_t* out);
TDYN_API const tdyn_symbol_info_t* tdyn_library_find_symbol_info_ptr(
    const tdyn_library_t* library,
    const char* name,
    tdyn_symbol_table_t table);
TDYN_API size_t tdyn_library_symbol_count(const tdyn_library_t* library,
                                          tdyn_symbol_table_t table);
TDYN_API tdyn_status_t tdyn_library_symbol_info(
    const tdyn_library_t* library,
    tdyn_symbol_table_t table,
    size_t index,
    tdyn_symbol_info_t* out);
TDYN_API tdyn_status_t tdyn_library_symbol_at(
    const tdyn_library_t* library,
    uint64_t address,
    tdyn_symbol_table_t table,
    tdyn_symbol_info_t* out);

TDYN_API size_t tdyn_library_needed_count(const tdyn_library_t* library);
TDYN_API const char* tdyn_library_needed(const tdyn_library_t* library,
                                         size_t index);

TDYN_API size_t tdyn_library_section_count(const tdyn_library_t* library);
TDYN_API tdyn_status_t tdyn_library_section_info(
    const tdyn_library_t* library,
    size_t index,
    tdyn_section_info_t* out);

TDYN_API size_t tdyn_library_relocation_count(const tdyn_library_t* library);
TDYN_API tdyn_status_t tdyn_library_relocation_info(
    const tdyn_library_t* library,
    size_t index,
    tdyn_relocation_info_t* out);

TDYN_API tdyn_status_t tdyn_library_file_offset_for_address(
    const tdyn_library_t* library,
    uint64_t address,
    uint64_t* out);
TDYN_API tdyn_status_t tdyn_library_bytes_at_address(
    const tdyn_library_t* library,
    uint64_t address,
    uint64_t length,
    const uint8_t** out_bytes,
    size_t* out_size);

TDYN_API tdyn_status_t tdyn_env_insert_symbol(tdyn_env_t* env,
                                              const char* name,
                                              void* address);
// The promoted library must have been opened by this environment.
TDYN_API tdyn_status_t tdyn_env_promote(tdyn_env_t* env,
                                        tdyn_library_t* library,
                                        tdyn_symbol_table_t table);
TDYN_API tdyn_status_t tdyn_env_symbol(tdyn_env_t* env,
                                       const char* name,
                                       void** out,
                                       tdyn_library_t** owner);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // TOURCHSTONE_DYNLOAD_H
