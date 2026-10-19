/*
 * DSymspec.h — typed, validated model of a DomSymAsCC architecture
 * specification (.dsymspec.yaml, schema 1; see ArchDB/schema/).
 *
 * Load:      dsym_spec_load_file("ArchDB/x86.dsymspec.yaml", flags, &diags)
 * Inspect:   spec->registers[i].name, dsym_spec_find_register(spec, "eax")
 * Encode:    dsym_encode(spec, enc_index, target_index, args, &out)
 * Serialise: dsym_spec_write(spec, DSYM_FORMAT_JSON, flags, &buf, &diags)
 *
 * Every string, array and sub-record belongs to the spec's arena and is
 * released by dsym_spec_free().  Optional scalar fields carry a has_* flag;
 * optional strings are NULL when absent; optional lists carry `present` so
 * that an explicit empty list survives a round-trip.
 *
 * Name references (form -> mnemonic/encoder/features/syntaxes,
 * register -> parent, target -> features, EMIT_RELOCATION -> relocs,
 * asmtk encoding -> encoder/register) are resolved into *_index fields
 * (-1 = absent).
 */
#ifndef METAARCH_DSYMSPEC_H
#define METAARCH_DSYMSPEC_H

#include "DsymspecIR.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DSYM_SCHEMA_VERSION 1
#define DSYM_NAME_MAX 128

/* ---- enumerations (order == schema order; *_names[] tables below) ------ */

typedef enum DsymByteOrder {
    DSYM_BO_LITTLE = 0,
    DSYM_BO_LE,
    DSYM_BO_BIG,
    DSYM_BO_BE
} DsymByteOrder;

typedef enum DsymRegClass {
    DSYM_RC_GPR = 0,
    DSYM_RC_SEG,
    DSYM_RC_CTRL,
    DSYM_RC_DEBUG,
    DSYM_RC_VEC,
    DSYM_RC_MASK,
    DSYM_RC_FP,
    DSYM_RC_STATUS,
    DSYM_RC_SPECIAL
} DsymRegClass;

enum { /* register flags (bit = 1 << enum) */
    DSYM_REGF_READABLE = 0,
    DSYM_REGF_WRITABLE,
    DSYM_REGF_IMP_USED,
    DSYM_REGF_IMP_MOD,
    DSYM_REGF_MODEDEP
};

enum { /* form flags */
    DSYM_FORMF_PRIVILEGED = 0,
    DSYM_FORMF_BRANCH,
    DSYM_FORMF_COND_BRANCH,
    DSYM_FORMF_RETURN,
    DSYM_FORMF_TERMINATOR,
    DSYM_FORMF_MAY_TRAP,
    DSYM_FORMF_READS_MEM,
    DSYM_FORMF_WRITES_MEM,
    DSYM_FORMF_DELAYED,
    DSYM_FORMF_IMPLICIT_OPS,
    DSYM_FORMF_RELOC,
    DSYM_FORMF_ALIASES
};

typedef enum DsymOperandKind {
    DSYM_OK_REG = 0,
    DSYM_OK_IMM,
    DSYM_OK_DISP,
    DSYM_OK_MEM,
    DSYM_OK_ABS,
    DSYM_OK_REL,
    DSYM_OK_SYM,
    DSYM_OK_COND,
    DSYM_OK_REGLIST,
    DSYM_OK_IMPLICIT
} DsymOperandKind;

typedef enum DsymAccess {
    DSYM_ACC_READ = 0,
    DSYM_ACC_WRITE,
    DSYM_ACC_READWRITE
} DsymAccess;

enum { /* section flags */
    DSYM_SECF_EXEC = 0,
    DSYM_SECF_WRITE,
    DSYM_SECF_ALLOC,
    DSYM_SECF_READONLY,
    DSYM_SECF_ZEROFILL,
    DSYM_SECF_MERGE,
    DSYM_SECF_DISCARD,
    DSYM_SECF_TLS,
    DSYM_SECF_DEBUG,
    DSYM_SECF_CODE,
    DSYM_SECF_DATA,
    DSYM_SECF_META
};

typedef enum DsymContent {
    DSYM_CONTENT_UNKNOWN = 0,
    DSYM_CONTENT_CODE,
    DSYM_CONTENT_DATA,
    DSYM_CONTENT_META
} DsymContent;

typedef enum DsymBinding {
    DSYM_BIND_GLOBAL = 0,
    DSYM_BIND_LOCAL,
    DSYM_BIND_WEAK,
    DSYM_BIND_TLS
} DsymBinding;

typedef enum DsymVisibility {
    DSYM_VIS_DEFAULT = 0,
    DSYM_VIS_HIDDEN,
    DSYM_VIS_PROTECTED,
    DSYM_VIS_INTERNAL
} DsymVisibility;

typedef enum DsymSymType {
    DSYM_SYM_FUNC = 0,
    DSYM_SYM_OBJECT,
    DSYM_SYM_NONE,
    DSYM_SYM_NOTYPE,
    DSYM_SYM_SECTION,
    DSYM_SYM_FILE
} DsymSymType;

typedef enum DsymOverflow {
    DSYM_OVF_ERROR = 0,
    DSYM_OVF_TRUNCATE
} DsymOverflow;

typedef enum DsymAddend {
    DSYM_ADD_EXPLICIT = 0,
    DSYM_ADD_INFIELD,
    DSYM_ADD_IMPLIED
} DsymAddend;

typedef enum DsymDebugKind {
    DSYM_DBG_TARGET = 0,
    DSYM_DBG_REGISTER,
    DSYM_DBG_FEATURE,
    DSYM_DBG_MNEMONIC,
    DSYM_DBG_FORM,
    DSYM_DBG_OPERAND,
    DSYM_DBG_ENCODER,
    DSYM_DBG_SYNTAX,
    DSYM_DBG_SECTION,
    DSYM_DBG_LINKAGE,
    DSYM_DBG_RELOC
} DsymDebugKind;

typedef enum DsymAsmForm {
    DSYM_AF_FIXED = 0,
    DSYM_AF_RELATIVE,
    DSYM_AF_INDIRECT,
    DSYM_AF_OPCODE_REGISTER,
    DSYM_AF_UNARY,
    DSYM_AF_REGISTER_REGISTER,
    DSYM_AF_LOAD,
    DSYM_AF_STORE,
    DSYM_AF_MOVE_IMMEDIATE,
    DSYM_AF_SHIFT_CL,
    DSYM_AF_SHIFT_IMMEDIATE,
    DSYM_AF_ARITHMETIC_IMMEDIATE
} DsymAsmForm;

typedef enum DsymSemantic {
    DSYM_SEM_ADD = 0,
    DSYM_SEM_SUB,
    DSYM_SEM_MUL,
    DSYM_SEM_AND,
    DSYM_SEM_OR,
    DSYM_SEM_XOR
} DsymSemantic;

/* ---- generic containers ------------------------------------------------- */

typedef struct DsymStrList {
    const char **items;
    size_t count;
    bool present; /* key appeared (possibly empty) */
} DsymStrList;

typedef struct DsymIntList {
    int64_t *items;
    size_t count;
    bool present;
} DsymIntList;

/* ---- records ------------------------------------------------------------ */

typedef struct DsymTarget {
    const char *arch;
    const char *subarch;
    int64_t addr_size;
    int64_t op_size;
    bool has_op_size;
    int64_t addr_def;
    bool has_addr_def;
    int64_t align;
    bool has_align;
    DsymByteOrder byte_order;
    const char *obj_format;
    const char *abi;
    const char *calling_conv;
    DsymStrList features;
    int32_t *feature_index; /* features.count entries */
} DsymTarget;

typedef struct DsymFeature {
    const char *name;
    const char *desc;
} DsymFeature;

typedef struct DsymRegister {
    const char *name;
    DsymRegClass reg_class;
    int64_t width;
    int64_t enc;
    const char *parent;
    int32_t parent_index;
    int64_t sub_lo;
    bool has_sub_lo;
    int64_t sub_hi;
    bool has_sub_hi;
    uint32_t flags; /* 1 << DSYM_REGF_* */
    bool has_flags;
} DsymRegister;

typedef struct DsymMnemonic {
    const char *name;
    DsymStrList aliases;
    bool ci;
    bool has_ci;
} DsymMnemonic;

typedef struct DsymEncoder {
    const char *name;
    int64_t max_bytes;
    bool has_max_bytes;
    int64_t max_relocs;
    bool has_max_relocs;
    const char *body;
    const DsymProgram *program; /* compiled body */
} DsymEncoder;

typedef struct DsymOperand {
    DsymOperandKind kind;
    int64_t width;
    bool has_width;
    DsymRegClass reg_class;
    bool has_reg_class;
    int64_t imm_min;
    bool has_imm_min;
    int64_t imm_max;
    bool has_imm_max;
    bool is_signed;
    bool has_signed;
    int64_t role;
    bool has_role;
    DsymAccess access;
    bool has_access;
    bool hidden;
    bool has_hidden;
    const char *constraint;
} DsymOperand;

typedef struct DsymForm {
    const char *mnemonic;
    int32_t mnemonic_index;
    const char *encoder;
    int32_t encoder_index;
    int64_t ops;
    bool has_ops;
    uint32_t flags; /* 1 << DSYM_FORMF_* */
    bool has_flags;
    DsymStrList required;
    int32_t *required_index;
    DsymStrList forbidden;
    int32_t *forbidden_index;
    DsymStrList syntaxes;
    int32_t *syntax_index;
    DsymOperand *operands;
    size_t operand_count;
    bool has_operands;
} DsymForm;

typedef struct DsymSyntax {
    const char *name;
    int64_t order; /* 1 = reversed operands (AT&T) */
    bool has_order;
    const char *reg_prefix;
    const char *imm_prefix;
    const char *comment;
    const char *separator;
    const char *tmpl;
    bool ci;
    bool has_ci;
} DsymSyntax;

typedef struct DsymSection {
    const char *name;
    uint32_t flags; /* 1 << DSYM_SECF_* */
    bool has_flags;
    int64_t align;
    bool has_align;
    int64_t addrspace;
    bool has_addrspace;
    DsymContent content;
    bool has_content;
} DsymSection;

typedef struct DsymLinkage {
    const char *name;
    DsymBinding binding;
    bool has_binding;
    DsymVisibility visibility;
    bool has_visibility;
    DsymSymType symtype;
    bool has_symtype;
} DsymLinkage;

typedef struct DsymReloc {
    const char *name;
    int64_t width;
    int64_t offset;
    bool has_offset;
    bool is_signed;
    bool has_signed;
    bool pcrel;
    bool has_pcrel;
    DsymOverflow overflow;
    bool has_overflow;
    DsymAddend addend;
    bool has_addend;
    const char *expr;
} DsymReloc;

typedef struct DsymDebug {
    DsymDebugKind kind;
    const char *msg;
    const char *ref;
} DsymDebug;

typedef struct DsymAsmEncoding {
    const char *mnemonic;
    DsymAsmForm form;
    DsymIntList widths; /* subset of {8, 32, 64} */
    DsymIntList opcode; /* 1..3 bytes */
    const char *encoder;
    int32_t encoder_index;
    int64_t group;
    bool has_group;
    int64_t reg_operand;
    bool has_reg_operand;
    int64_t relocation;
    bool has_relocation;
    bool rex_w;
    bool has_rex_w;
    const char *implicit_register;
    int32_t implicit_register_index;
} DsymAsmEncoding;

typedef struct DsymAsmCondition {
    int64_t code;
    DsymStrList names;
} DsymAsmCondition;

typedef struct DsymAsmtk {
    int64_t schema;
    DsymAsmEncoding *encodings;
    size_t encoding_count;
    DsymAsmCondition *conditions;
    size_t condition_count;
} DsymAsmtk;

typedef struct DsymLowering {
    const char *copy;
    const char *ret; /* key "return" */
    const char *stack_pointer;
    const char *accumulator;
    const char *scratch;
} DsymLowering;

typedef struct DsymAllocation {
    DsymStrList reserved;
    DsymStrList callee_saved;
    DsymStrList arguments;
    DsymStrList results;
    int64_t stack_alignment;
} DsymAllocation;

typedef struct DsymValue {
    const char *symbol;
    DsymSemantic semantic;
    const char *mnemonic;
    const char *instruction;
    const char *pattern;
} DsymValue;

typedef struct DsymMetachain {
    int64_t schema;
    const char *scheduling_model;
    const char *register_class;
    DsymLowering *lowering;
    DsymAllocation *allocation;
    DsymValue *values;
    size_t value_count;
} DsymMetachain;

typedef struct DsymBackend {
    const char *file;   /* relative to the spec file; NULL if embedded */
    DsymNode *document; /* loaded/embedded backend JSON; NULL if not loaded */
} DsymBackend;

typedef struct DsymSpec {
    DsymArena *arena;
    const char *source_path; /* file it was loaded from, if any */
    const char *base_dir;    /* directory of source_path ("." if none) */

    int64_t schema;
    DsymTarget *targets;
    size_t target_count;
    DsymFeature *features;
    size_t feature_count;
    bool has_features;
    DsymRegister *registers;
    size_t register_count;
    bool has_registers;
    DsymMnemonic *mnemonics;
    size_t mnemonic_count;
    bool has_mnemonics;
    DsymEncoder *encoders;
    size_t encoder_count;
    bool has_encoders;
    DsymForm *forms;
    size_t form_count;
    bool has_forms;
    DsymSyntax *syntaxes;
    size_t syntax_count;
    bool has_syntaxes;
    DsymSection *sections;
    size_t section_count;
    bool has_sections;
    DsymLinkage *linkages;
    size_t linkage_count;
    bool has_linkages;
    DsymReloc *relocs;
    size_t reloc_count;
    bool has_relocs;
    DsymDebug *debug;
    size_t debug_count;
    bool has_debug;
    DsymAsmtk *asmtk;         /* optional */
    DsymMetachain *metachain; /* optional */
    DsymBackend *backend;     /* optional */

    struct DsymIndex *index; /* private name lookup tables */
} DsymSpec;

/* ---- loading ------------------------------------------------------------ */

enum {
    DSYM_LOAD_BACKEND = 1u << 0,    /* read backend/<arch>.json */
    DSYM_LOAD_NO_COMPILE = 1u << 1, /* skip compiling encoder bodies */
    DSYM_LOAD_STRICT = 1u << 2,     /* warnings count as errors */
    DSYM_LOAD_LINT = 1u << 3        /* extra checks (max_bytes, ...) */
};

/* Format is auto-detected from the extension unless given explicitly. */
DsymSpec *dsym_spec_load_file(const char *path, DsymFormat fmt, uint32_t flags,
                              DsymDiags *diags);
DsymSpec *dsym_spec_load_buffer(const char *data, size_t len, DsymFormat fmt,
                                const char *virtual_path, uint32_t flags,
                                DsymDiags *diags);
/* Bind an already-parsed tree (the tree may live in another arena). */
DsymSpec *dsym_spec_from_tree(const DsymNode *root, const char *virtual_path,
                              uint32_t flags, DsymDiags *diags);
/* Load the referenced backend document now (if not already loaded). */
bool dsym_spec_load_backend(DsymSpec *spec, DsymDiags *diags);
void dsym_spec_free(DsymSpec *spec);

/* Empty spec for programmatic construction (fill fields with arena memory,
   then call dsym_spec_resolve()). */
DsymSpec *dsym_spec_new(void);
/* (Re)validate cross references and (re)build lookup tables and programs. */
bool dsym_spec_resolve(DsymSpec *spec, uint32_t flags, DsymDiags *diags);

/* ---- serialisation ------------------------------------------------------ */

/* Model -> canonical document tree (schema key order). */
DsymNode *dsym_spec_to_tree(const DsymSpec *spec, DsymArena *a,
                            uint32_t write_flags);
/* JSON embeds a loaded backend document by default (schema: "merged into
   JSON output as `backend`"); other formats reference it unless
   DSYM_WRITE_EMBED_BACKEND is given. */
bool dsym_spec_write(const DsymSpec *spec, DsymFormat fmt, uint32_t flags,
                     DsymBuf *out, DsymDiags *diags);
bool dsym_spec_write_file(const DsymSpec *spec, const char *path,
                          DsymFormat fmt, uint32_t flags, DsymDiags *diags);

/* ---- lookup (O(1) hash tables; -1 when not found) ----------------------- */

int32_t dsym_spec_find_feature(const DsymSpec *s, const char *name);
int32_t dsym_spec_find_register(const DsymSpec *s, const char *name);
int32_t dsym_spec_find_mnemonic(const DsymSpec *s, const char *name);
/* Resolves aliases; case-insensitive for mnemonics with ci: true. */
int32_t dsym_spec_find_mnemonic_or_alias(const DsymSpec *s, const char *name);
int32_t dsym_spec_find_encoder(const DsymSpec *s, const char *name);
int32_t dsym_spec_find_syntax(const DsymSpec *s, const char *name);
int32_t dsym_spec_find_section(const DsymSpec *s, const char *name);
int32_t dsym_spec_find_linkage(const DsymSpec *s, const char *name);
int32_t dsym_spec_find_reloc(const DsymSpec *s, const char *name);
/* Forms of a mnemonic: fills up to `cap` indices, returns the total. */
size_t dsym_spec_forms_for_mnemonic(const DsymSpec *s, int32_t mnemonic_index,
                                    int32_t *out, size_t cap);
bool dsym_target_is_big_endian(const DsymTarget *t);

/* ---- enum spelling ------------------------------------------------------ */

const char *dsym_byte_order_name(DsymByteOrder v);
const char *dsym_reg_class_name(DsymRegClass v);
const char *dsym_operand_kind_name(DsymOperandKind v);
const char *dsym_access_name(DsymAccess v);
const char *dsym_content_name(DsymContent v);
const char *dsym_binding_name(DsymBinding v);
const char *dsym_visibility_name(DsymVisibility v);
const char *dsym_symtype_name(DsymSymType v);
const char *dsym_overflow_name(DsymOverflow v);
const char *dsym_addend_name(DsymAddend v);
const char *dsym_debug_kind_name(DsymDebugKind v);
const char *dsym_asm_form_name(DsymAsmForm v);
const char *dsym_semantic_name(DsymSemantic v);
const char *dsym_reg_flag_name(unsigned bit);
const char *dsym_form_flag_name(unsigned bit);
const char *dsym_section_flag_name(unsigned bit);

/* ---- encoding ----------------------------------------------------------- */

#define DSYM_ENCODE_MAX_BYTES 64
#define DSYM_ENCODE_MAX_RELOCS 8

typedef struct DsymEncodedReloc {
    uint32_t offset;     /* byte offset of the field in the output */
    uint32_t size;       /* field size in bytes */
    int32_t reloc_index; /* into spec->relocs */
    const char *reloc_name;
    uint32_t sym_param; /* operand slot of the symbol */
    int64_t sym_value;  /* value bound to that operand */
} DsymEncodedReloc;

typedef struct DsymEncoded {
    uint8_t bytes[DSYM_ENCODE_MAX_BYTES];
    size_t size;
    DsymEncodedReloc relocs[DSYM_ENCODE_MAX_RELOCS];
    size_t reloc_count;
    char error[160]; /* set when dsym_encode returns false */
} DsymEncoded;

/* args[i] binds program->params[i]. target_index selects byte order for
   EMIT_IMM (0 if unsure). */
bool dsym_encode(const DsymSpec *s, int32_t encoder_index, size_t target_index,
                 const int64_t *args, size_t arg_count, DsymEncoded *out);
/* Convenience: bind by name; unbound parameters default to 0 and are
   reported as an error unless `allow_unbound`. */
bool dsym_encode_named(const DsymSpec *s, const char *encoder,
                       size_t target_index, const char *const *names,
                       const int64_t *values, size_t n, bool allow_unbound,
                       DsymEncoded *out);

#ifdef __cplusplus
}
#endif
#endif /* METAARCH_DSYMSPEC_H */
