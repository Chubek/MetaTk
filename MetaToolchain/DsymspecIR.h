/*
 * DsymspecIR.h — intermediate representations shared by the MetaArch codecs.
 *
 * Two IRs live here:
 *
 *   1. The *document tree* (DsymNode): a format-neutral, ordered, typed value
 *      tree (null/bool/int/uint/real/string/seq/map) with source positions.
 *      Every codec (YAML, JSON, XML, S-expression) converts between its wire
 *      format and this tree; the schema binder in Dsymspec.c converts between
 *      the tree and the typed model in DSymspec.h.  Adding a format therefore
 *      only needs a tree reader/writer.
 *
 *   2. The *encoder program* (DsymProgram): encoder bodies compiled from the
 *      imperative pseudo-code into a statement list whose expressions are
 *      stack-machine (RPN) bytecode with constant folding applied.
 *
 * Plus the small infrastructure both need: an arena allocator, a growable
 * byte buffer, and a diagnostics list.
 */
#ifndef METAARCH_DSYMSPEC_IR_H
#define METAARCH_DSYMSPEC_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* Arena allocator: everything a DsymSpec / tree owns is freed in one go.   */
/* ------------------------------------------------------------------------ */

typedef struct DsymArena DsymArena;

DsymArena *dsym_arena_new(void);
void dsym_arena_free(DsymArena *a);
/* Zero-initialised, max_align_t aligned. Returns NULL only on OOM. */
void *dsym_arena_alloc(DsymArena *a, size_t size);
void *dsym_arena_calloc(DsymArena *a, size_t n, size_t size);
/* NUL-terminated copy of s[0..len). */
char *dsym_arena_strndup(DsymArena *a, const char *s, size_t len);
char *dsym_arena_strdup(DsymArena *a, const char *s);
char *dsym_arena_printf(DsymArena *a, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

/* ------------------------------------------------------------------------ */
/* Growable byte buffer used as the sink of every writer.                   */
/* ------------------------------------------------------------------------ */

typedef struct DsymBuf {
    char *data; /* always NUL-terminated when non-NULL */
    size_t len;
    size_t cap;
    bool oom; /* sticky allocation failure */
} DsymBuf;

void dsym_buf_init(DsymBuf *b);
void dsym_buf_free(DsymBuf *b);
bool dsym_buf_reserve(DsymBuf *b, size_t extra);
bool dsym_buf_append(DsymBuf *b, const void *data, size_t len);
bool dsym_buf_puts(DsymBuf *b, const char *s);
bool dsym_buf_putc(DsymBuf *b, char c);
bool dsym_buf_printf(DsymBuf *b, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
/* Read a whole file. Returns false (and sets errno) on failure. */
bool dsym_buf_read_file(DsymBuf *b, const char *path);
bool dsym_buf_write_file(const DsymBuf *b, const char *path);

/* ------------------------------------------------------------------------ */
/* Diagnostics.                                                             */
/* ------------------------------------------------------------------------ */

typedef enum DsymSeverity {
    DSYM_NOTE = 0,
    DSYM_WARNING = 1,
    DSYM_ERROR = 2
} DsymSeverity;

typedef struct DsymDiag {
    DsymSeverity severity;
    const char *file; /* may be NULL */
    uint32_t line;    /* 1-based; 0 = unknown */
    uint32_t col;     /* 1-based; 0 = unknown */
    const char *path; /* JSON-pointer-ish location, e.g. /forms/3/encoder */
    const char *msg;
} DsymDiag;

typedef struct DsymDiags {
    DsymArena *arena; /* owns strings */
    DsymDiag *items;
    size_t count;
    size_t cap;
    size_t errors;
    size_t warnings;
    size_t max_errors; /* 0 = unlimited; extra errors are counted, not stored */
} DsymDiags;

void dsym_diags_init(DsymDiags *d);
void dsym_diags_free(DsymDiags *d);
void dsym_diag_add(DsymDiags *d, DsymSeverity sev, const char *file,
                   uint32_t line, uint32_t col, const char *path,
                   const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 7, 8)))
#endif
    ;
/* "file:line:col: error: msg [at path]" lines. */
void dsym_diags_print(const DsymDiags *d, FILE *out);

/* ------------------------------------------------------------------------ */
/* Document tree.                                                           */
/* ------------------------------------------------------------------------ */

typedef enum DsymNodeKind {
    DSYM_NODE_NULL = 0,
    DSYM_NODE_BOOL,
    DSYM_NODE_INT,  /* fits int64_t */
    DSYM_NODE_UINT, /* > INT64_MAX, stored in u */
    DSYM_NODE_REAL,
    DSYM_NODE_STR,
    DSYM_NODE_SEQ,
    DSYM_NODE_MAP
} DsymNodeKind;

/* Presentation hints set by readers and honoured by writers when possible. */
enum {
    DSYM_HINT_PLAIN = 1u << 0,   /* YAML plain scalar (text is raw source) */
    DSYM_HINT_QUOTED = 1u << 1,  /* scalar was quoted in the source */
    DSYM_HINT_LITERAL = 1u << 2, /* prefer YAML literal block (|) */
    DSYM_HINT_FLOW = 1u << 3,    /* prefer YAML flow style */
    DSYM_HINT_BLOCK = 1u << 4,   /* prefer YAML block style */
    DSYM_HINT_HEX = 1u << 5      /* integer was written in hex */
};

typedef struct DsymNode DsymNode;
typedef struct DsymPair {
    const char *key;
    size_t key_len;
    DsymNode *value;
} DsymPair;

struct DsymNode {
    DsymNodeKind kind;
    uint32_t hints;
    uint32_t line, col; /* 1-based source position, 0 if unknown */
    /* Original scalar spelling for scalars that came from text formats
       (YAML plain scalars, numbers). NULL when not applicable. Lets the binder
       accept e.g. `desc: 3` for a string field without loss. */
    const char *text;
    union {
        bool b;
        int64_t i;
        uint64_t u;
        double r;
        struct {
            const char *ptr;
            size_t len;
        } str;
        struct {
            DsymNode **items;
            size_t count, cap;
        } seq;
        struct {
            DsymPair *items;
            size_t count, cap;
        } map;
    } v;
};

DsymNode *dsym_node_new(DsymArena *a, DsymNodeKind kind);
DsymNode *dsym_node_null(DsymArena *a);
DsymNode *dsym_node_bool(DsymArena *a, bool b);
DsymNode *dsym_node_int(DsymArena *a, int64_t i);
DsymNode *dsym_node_uint(DsymArena *a, uint64_t u); /* normalises to INT */
DsymNode *dsym_node_real(DsymArena *a, double r);
DsymNode *dsym_node_str(DsymArena *a, const char *s);              /* copies */
DsymNode *dsym_node_strn(DsymArena *a, const char *s, size_t len); /* copies */
DsymNode *dsym_node_str_ref(DsymArena *a, const char *s);  /* no copy */
DsymNode *dsym_node_seq(DsymArena *a);
DsymNode *dsym_node_map(DsymArena *a);
bool dsym_seq_push(DsymArena *a, DsymNode *seq, DsymNode *item);
/* Appends; does not check for duplicates (readers do that). key is copied. */
bool dsym_map_put(DsymArena *a, DsymNode *map, const char *key, size_t key_len,
                  DsymNode *value);
/* key is NOT copied: it must outlive the tree. */
bool dsym_map_put_ref(DsymArena *a, DsymNode *map, const char *key,
                      DsymNode *value);
DsymNode *dsym_map_get(const DsymNode *map, const char *key);
const char *dsym_node_kind_name(DsymNodeKind k);
/* Deep structural equality (ignores hints/positions/text). */
bool dsym_node_equal(const DsymNode *a, const DsymNode *b);
/* Deep copy into another arena. */
DsymNode *dsym_node_clone(DsymArena *a, const DsymNode *n);

/* ------------------------------------------------------------------------ */
/* Tree codecs (implemented in ToYAML.c, ToJSON.c, ToXML.c, ToSExpr.c).     */
/* Readers return NULL on failure with diagnostics added to `diags`.        */
/* ------------------------------------------------------------------------ */

typedef enum DsymFormat {
    DSYM_FORMAT_AUTO = 0,
    DSYM_FORMAT_YAML,
    DSYM_FORMAT_JSON,
    DSYM_FORMAT_XML,
    DSYM_FORMAT_SEXPR
} DsymFormat;

enum {
    DSYM_WRITE_COMPACT = 1u << 0,        /* minimal whitespace */
    DSYM_WRITE_EMBED_BACKEND = 1u << 1,  /* inline backend document */
    DSYM_WRITE_REF_BACKEND = 1u << 2,    /* force `backend: {file: ...}` */
    DSYM_WRITE_NO_HEADER = 1u << 3,      /* no generated-by comment */
    DSYM_WRITE_SORT_KEYS = 1u << 4       /* (generic trees only) */
};

DsymNode *dsym_yaml_read(DsymArena *a, const char *data, size_t len,
                         const char *file, DsymDiags *diags);
bool dsym_yaml_write(const DsymNode *root, DsymBuf *out, uint32_t flags,
                     DsymDiags *diags);

DsymNode *dsym_json_read(DsymArena *a, const char *data, size_t len,
                         const char *file, DsymDiags *diags);
bool dsym_json_write(const DsymNode *root, DsymBuf *out, uint32_t flags,
                     DsymDiags *diags);

/* XML uses the W3C XPath 3.1 "json-to-xml" vocabulary
   (namespace http://www.w3.org/2005/xpath-functions): <map>, <array>,
   <string>, <number>, <boolean>, <null>, with key="" on map members.
   It is lossless and convertible with XSLT 3.0 xml-to-json(). */
DsymNode *dsym_xml_read(DsymArena *a, const char *data, size_t len,
                        const char *file, DsymDiags *diags);
bool dsym_xml_write(const DsymNode *root, DsymBuf *out, uint32_t flags,
                    DsymDiags *diags);

/* S-expressions: maps are keyword plists `(:key value ...)`, sequences are
   lists `(a b c)`, strings are "quoted", integers/reals are bare, and
   #t / #f / #nil are booleans and null.  `(:)` is the empty map. */
DsymNode *dsym_sexpr_read(DsymArena *a, const char *data, size_t len,
                          const char *file, DsymDiags *diags);
bool dsym_sexpr_write(const DsymNode *root, DsymBuf *out, uint32_t flags,
                      DsymDiags *diags);

DsymFormat dsym_format_from_name(const char *name); /* "yaml", "json", ... */
DsymFormat dsym_format_from_path(const char *path); /* by extension */
const char *dsym_format_name(DsymFormat f);
const char *dsym_format_extension(DsymFormat f); /* ".yaml", ".json", ... */
DsymNode *dsym_tree_read(DsymFormat f, DsymArena *a, const char *data,
                         size_t len, const char *file, DsymDiags *diags);
bool dsym_tree_write(DsymFormat f, const DsymNode *root, DsymBuf *out,
                     uint32_t flags, DsymDiags *diags);

/* ------------------------------------------------------------------------ */
/* Encoder program IR.                                                      */
/*                                                                          */
/* Grammar (one statement per line, `#` starts a comment):                  */
/*   EMIT8 e | EMIT16LE e | EMIT16BE e | EMIT32LE e | EMIT32BE e            */
/*   EMIT64LE e | EMIT64BE e                                                */
/*   EMIT_IMM n, e         n in {1,2,4,8}; target byte order                */
/*   EMIT_MODRM reg, rm    one byte 0xC0 | (reg&7)<<3 | (rm&7)              */
/*   EMIT_RELOCATION sym, RELOC_NAME   relocation on the last emitted field */
/*   REQUIRE cond                                                           */
/* Expressions: integer literals (dec, 0x, 0b, 0o), operand identifiers,    */
/*   unary - ~ !, * / %, + -, << >>, < <= > >=, == !=, &, ^, |, &&, ||,     */
/*   parentheses.  Arithmetic is 64-bit two's complement (wrapping); / and  */
/*   % are signed, % is Euclidean (result >= 0 for positive divisors).      */
/* ------------------------------------------------------------------------ */

typedef enum DsymOp {
    DSYM_OP_CONST = 0, /* push imm */
    DSYM_OP_PARAM,     /* push args[imm] */
    DSYM_OP_NEG,
    DSYM_OP_BNOT,
    DSYM_OP_LNOT,
    DSYM_OP_MUL,
    DSYM_OP_DIV,
    DSYM_OP_MOD,
    DSYM_OP_ADD,
    DSYM_OP_SUB,
    DSYM_OP_SHL,
    DSYM_OP_SHR, /* arithmetic */
    DSYM_OP_LT,
    DSYM_OP_LE,
    DSYM_OP_GT,
    DSYM_OP_GE,
    DSYM_OP_EQ,
    DSYM_OP_NE,
    DSYM_OP_BAND,
    DSYM_OP_BXOR,
    DSYM_OP_BOR,
    DSYM_OP_LAND,
    DSYM_OP_LOR,
    DSYM_OP__COUNT
} DsymOp;

typedef struct DsymInsn {
    DsymOp op;
    int64_t imm;
} DsymInsn;

typedef struct DsymExprRef {
    uint32_t start; /* index into program->code */
    uint32_t len;
} DsymExprRef;

typedef enum DsymStmtKind {
    DSYM_ST_EMIT = 0,   /* fixed-width field, explicit endianness */
    DSYM_ST_EMIT_IMM,   /* fixed-width field, target endianness */
    DSYM_ST_EMIT_MODRM, /* x86 register-direct ModR/M byte */
    DSYM_ST_RELOC,      /* relocation against last field */
    DSYM_ST_REQUIRE     /* operand constraint */
} DsymStmtKind;

typedef enum DsymEndian {
    DSYM_ENDIAN_LITTLE = 0,
    DSYM_ENDIAN_BIG = 1,
    DSYM_ENDIAN_TARGET = 2
} DsymEndian;

typedef struct DsymStmt {
    DsymStmtKind kind;
    uint8_t size;      /* bytes emitted (EMIT*, EMIT_IMM, MODRM) */
    DsymEndian endian; /* EMIT / EMIT_IMM */
    uint32_t line;     /* line within the body, 1-based */
    DsymExprRef expr[2];
    uint32_t sym_param;     /* RELOC: parameter slot naming the symbol */
    const char *reloc_name; /* RELOC */
    int32_t reloc_index;    /* RELOC: index into spec->relocs, -1 unresolved */
    const char *text;       /* original statement text */
} DsymStmt;

typedef struct DsymProgram {
    const DsymStmt *stmts;
    size_t stmt_count;
    const DsymInsn *code;
    size_t code_count;
    const char *const *params; /* operand names, in first-use order */
    size_t param_count;
    uint32_t static_size;  /* bytes always emitted */
    uint32_t reloc_count;  /* relocations always emitted */
    uint32_t max_stack;    /* evaluation stack depth needed */
} DsymProgram;

/* Compile `body` into a program. reloc lookup is optional (may be NULL). */
typedef int32_t (*DsymRelocLookup)(void *ctx, const char *name);
const DsymProgram *dsym_program_compile(DsymArena *a, const char *body,
                                        const char *file, uint32_t base_line,
                                        const char *path,
                                        DsymRelocLookup lookup, void *ctx,
                                        DsymDiags *diags);
int dsym_program_param_index(const DsymProgram *p, const char *name);
/* Evaluate one expression; returns false on stack/div-by-zero error. */
bool dsym_expr_eval(const DsymProgram *p, DsymExprRef e, const int64_t *args,
                    int64_t *result, const char **error);
/* Human-readable listing of the IR. */
void dsym_program_dump(const DsymProgram *p, DsymBuf *out);
const char *dsym_op_name(DsymOp op);

#ifdef __cplusplus
}
#endif
#endif /* METAARCH_DSYMSPEC_IR_H */
