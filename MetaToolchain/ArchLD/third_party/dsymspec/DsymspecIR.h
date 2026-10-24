/* Standalone JSON subset of DsymspecIR used by ArchBFD.
 * Not ABI-compatible with the full Dsymspec library; never mix the two.
 * The original ArchBFD.c/.h are included without modification.
 */
#ifndef ARCHLD_DSYMSPEC_COMPAT_H
#define ARCHLD_DSYMSPEC_COMPAT_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
typedef struct DsymArena DsymArena;
typedef enum {DSYM_FORMAT_AUTO=0, DSYM_FORMAT_JSON=1, DSYM_FORMAT_YAML=2, DSYM_FORMAT_XML=3,DSYM_FORMAT_SEXP=4} DsymFormat;
typedef enum {DSYM_NODE_NULL,DSYM_NODE_MAP,DSYM_NODE_SEQ,DSYM_NODE_STR,DSYM_NODE_INT,DSYM_NODE_UINT,DSYM_NODE_BOOL} DsymNodeKind;
typedef struct DsymNode DsymNode;
typedef struct DsymMapPair {const char *key; DsymNode *value;} DsymMapPair;
struct DsymNode {DsymNodeKind kind; unsigned line,col; const char *text;
 union {struct {DsymMapPair *items; size_t count,cap;} map;
 struct {DsymNode **items;size_t count,cap;} seq;
 struct {const char *ptr;size_t len;} str; int64_t i; uint64_t u; bool b;} v;};
typedef struct DsymBuf {char *data;size_t len,cap;} DsymBuf;
typedef struct DsymDiag {int severity;unsigned line,col;const char *file,*path,*msg;} DsymDiag;
typedef struct DsymDiags {DsymDiag *items;size_t count,errors,warnings,cap;} DsymDiags;
enum {DSYM_ERROR=2, DSYM_WARNING=1, DSYM_WRITE_NO_HEADER=1};
#ifdef __cplusplus
extern "C" {
#endif
DsymArena *dsym_arena_new(void);
void dsym_arena_free(DsymArena*);
void *dsym_arena_alloc(DsymArena*,size_t);
void *dsym_arena_calloc(DsymArena*,size_t,size_t);
char *dsym_arena_strdup(DsymArena*,const char*);
char *dsym_arena_strndup(DsymArena*,const char*,size_t);
char *dsym_arena_printf(DsymArena*,const char*,...);
void dsym_diags_init(DsymDiags*);
void dsym_diags_free(DsymDiags*);
void dsym_diag_add(DsymDiags*,int,const char*,unsigned,unsigned,const char*,const char*,...);
void dsym_buf_init(DsymBuf*);
void dsym_buf_free(DsymBuf*);
bool dsym_buf_puts(DsymBuf*,const char*);
bool dsym_buf_read_file(DsymBuf*,const char*);
DsymFormat dsym_format_from_path(const char*);
DsymNode *dsym_tree_read(DsymFormat,DsymArena*,const char*,size_t,const char*,DsymDiags*);
bool dsym_tree_write(DsymFormat,const DsymNode*,DsymBuf*,uint32_t,DsymDiags*);
const DsymNode *dsym_map_get(const DsymNode*,const char*);
bool dsym_map_put_ref(DsymArena*,DsymNode*,const char*,DsymNode*);
bool dsym_seq_push(DsymArena*,DsymNode*,DsymNode*);
DsymNode *dsym_node_map(DsymArena*);
DsymNode *dsym_node_seq(DsymArena*);
DsymNode *dsym_node_int(DsymArena*,int64_t);
DsymNode *dsym_node_uint(DsymArena*,uint64_t);
DsymNode *dsym_node_bool(DsymArena*,bool);
DsymNode *dsym_node_str_ref(DsymArena*,const char*);
#ifdef __cplusplus
}
#endif
#endif
