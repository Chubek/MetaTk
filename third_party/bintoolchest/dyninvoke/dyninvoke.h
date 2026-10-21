// dyninvoke.h - Tourchstone foreign-call library (libffi/dyncall analogue).
//
// Lowering is driven by Yellowstone qBFD (Format/Arch -> ABI, see qABI.hpp)
// and cross-checked against Sandstone .isa register files. Implemented layout
// subsets are queryable cross-host; execution requires a host trampoline.
#ifndef TOURCHSTONE_DYNINVOKE_H
#define TOURCHSTONE_DYNINVOKE_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(TDYNINVOKE_SHARED)
#  ifdef tdyninvoke_EXPORTS
#    define DINV_API __declspec(dllexport)
#  else
#    define DINV_API __declspec(dllimport)
#  endif
#else
#  define DINV_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dinv_status {
  DINV_OK = 0,
  DINV_ERROR = 1,
  DINV_INVALID_ARGUMENT = 2,
  DINV_UNSUPPORTED = 3,
  DINV_NO_MEMORY = 4,
  DINV_LAYOUT_ERROR = 5,
  DINV_CALL_ERROR = 6,
  DINV_NOT_FOUND = 7
} dinv_status_t;

typedef enum dinv_abi {
  DINV_ABI_AUTO = 0,
  DINV_ABI_SYSV_AMD64 = 1,
  DINV_ABI_WIN64 = 2,
  DINV_ABI_AAPCS64 = 3,
  DINV_ABI_RISCV64 = 4,
  DINV_ABI_RISCV32 = 5
} dinv_abi_t;

typedef enum dinv_kind {
  DINV_VOID = 0,
  DINV_BOOL = 1,
  DINV_I8 = 2,
  DINV_U8 = 3,
  DINV_I16 = 4,
  DINV_U16 = 5,
  DINV_I32 = 6,
  DINV_U32 = 7,
  DINV_I64 = 8,
  DINV_U64 = 9,
  DINV_F32 = 10,
  DINV_F64 = 11,
  DINV_PTR = 12,
  DINV_STRUCT = 13
} dinv_kind_t;

typedef struct dinv_type dinv_type_t;
struct dinv_type {
  dinv_kind_t kind;
  const dinv_type_t* const* elements;  // NULL unless STRUCT (NULL-terminated)
};

DINV_API extern const dinv_type_t dinv_type_void;
DINV_API extern const dinv_type_t dinv_type_bool;
DINV_API extern const dinv_type_t dinv_type_i8;
DINV_API extern const dinv_type_t dinv_type_u8;
DINV_API extern const dinv_type_t dinv_type_i16;
DINV_API extern const dinv_type_t dinv_type_u16;
DINV_API extern const dinv_type_t dinv_type_i32;
DINV_API extern const dinv_type_t dinv_type_u32;
DINV_API extern const dinv_type_t dinv_type_i64;
DINV_API extern const dinv_type_t dinv_type_u64;
DINV_API extern const dinv_type_t dinv_type_f32;
DINV_API extern const dinv_type_t dinv_type_f64;
DINV_API extern const dinv_type_t dinv_type_ptr;

typedef struct dinv_cif dinv_cif_t;
typedef struct dinv_closure dinv_closure_t;
// Callback cif is borrowed from the closure; do not destroy or retain it beyond
// closure destruction. Callbacks must not throw across the native ABI boundary.
typedef void (*dinv_closure_fn_t)(dinv_cif_t* cif, void* ret, void** args,
                                  void* user);

typedef enum dinv_loc_kind {
  DINV_LOC_VOID = 0,
  DINV_LOC_GPR = 1,
  DINV_LOC_FPR = 2,
  DINV_LOC_STACK = 3
} dinv_loc_kind_t;

typedef struct dinv_slot {
  dinv_loc_kind_t where;
  int reg;                // GPR/FPR index, -1 for stack
  size_t stack_offset;    // valid when where == STACK
  size_t size;            // slot payload bytes
  size_t field_offset;    // byte offset inside the arg value
} dinv_slot_t;

DINV_API const char* dinv_version(void);
DINV_API const char* dinv_status_string(dinv_status_t status);
DINV_API const char* dinv_last_error(void);

DINV_API const char* dinv_abi_name(dinv_abi_t abi);
DINV_API dinv_status_t dinv_abi_for_name(const char* name, dinv_abi_t* out);
DINV_API dinv_abi_t dinv_abi_host(void);
// Nonzero only when this build provides execution for the selected host ABI.
DINV_API int dinv_abi_can_execute(dinv_abi_t abi);
DINV_API const char* dinv_abi_isa_arch(dinv_abi_t abi);
DINV_API const char* dinv_abi_isa_file(dinv_abi_t abi);

DINV_API const char* dinv_kind_name(dinv_kind_t kind);
DINV_API dinv_status_t dinv_type_for_name(const char* name,
                                          const dinv_type_t** out);
// Borrowed immutable builtin descriptor; NULL on failure (dinv_last_error).
DINV_API const dinv_type_t* dinv_type_named(const char* name);
DINV_API size_t dinv_type_size(const dinv_type_t* type);
DINV_API size_t dinv_type_align(const dinv_type_t* type);

// Precompute a call interface. `args` is an array of `nargs` type pointers
// (borrowed during creation). The cif owns a deep copy of the type graph and
// layout. Closures retain their own cif and may outlive the source cif.
DINV_API dinv_status_t dinv_cif_create(dinv_abi_t abi,
                                       const dinv_type_t* ret,
                                       size_t nargs,
                                       const dinv_type_t* const* args,
                                       dinv_cif_t** out);
DINV_API void dinv_cif_destroy(dinv_cif_t* cif);
DINV_API dinv_abi_t dinv_cif_abi(const dinv_cif_t* cif);
DINV_API size_t dinv_cif_nargs(const dinv_cif_t* cif);
DINV_API int dinv_cif_gpr_used(const dinv_cif_t* cif);
DINV_API int dinv_cif_fpr_used(const dinv_cif_t* cif);
DINV_API size_t dinv_cif_stack_bytes(const dinv_cif_t* cif);
DINV_API int dinv_cif_uses_hidden_ret(const dinv_cif_t* cif);

// Slots carrying argument `index`. When `slots` is NULL, `*inout_count`
// receives the required count; otherwise up to `*inout_count` slots are
// written and the count is updated.
DINV_API dinv_status_t dinv_cif_arg_slots(const dinv_cif_t* cif, size_t index,
                                          dinv_slot_t* slots,
                                          size_t* inout_count);

// `args` is an array of `nargs` pointers to argument values; `ret` points to
// storage for the return value (unused when return is void).
DINV_API dinv_status_t dinv_call(dinv_cif_t* cif, void* func, void* ret,
                                 void** args);

// One-shot call without managing a cif.
DINV_API dinv_status_t dinv_call_simple(dinv_abi_t abi,
                                        const dinv_type_t* ret, void* func,
                                        void* ret_out, size_t nargs,
                                        const dinv_type_t* const* arg_types,
                                        void** args);

// Closure (callback) support. Currently implemented for SysV AMD64; other
// ABIs return DINV_UNSUPPORTED. `*out_fn` is an executable function pointer
// valid until dinv_closure_destroy.
DINV_API dinv_status_t dinv_closure_create(dinv_cif_t* cif,
                                           dinv_closure_fn_t fn, void* user,
                                           dinv_closure_t** out_closure,
                                           void** out_fn);
DINV_API void dinv_closure_destroy(dinv_closure_t* closure);

// Yellowstone integration: infer the call ABI for an object file and check
// that `symbol` (optional, may be NULL) exists in its static/dynamic tables.
DINV_API dinv_status_t dinv_abi_for_object(const char* path,
                                           dinv_abi_t* out_abi);
DINV_API dinv_status_t dinv_inspect_object(const char* path,
                                           const char* symbol,
                                           dinv_abi_t* out_abi,
                                           int* out_found);

// Sandstone integration: cross-check the compiled-in register counts against
// the .isa register file. `isa_dir` defaults to the installed
// sandstone/isa directory when NULL. Counts are totals in the GPR/FPR
// regclasses (width-agnostic, aliases excluded).
DINV_API const char* dinv_isa_default_dir(void);
DINV_API dinv_status_t dinv_isa_regcounts(dinv_abi_t abi, const char* isa_dir,
                                          size_t* out_gpr,
                                          size_t* out_fpr);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // TOURCHSTONE_DYNINVOKE_H
