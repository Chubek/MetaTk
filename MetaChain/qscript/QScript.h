#ifndef METACHAIN_QSCRIPT_H
#define METACHAIN_QSCRIPT_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct qs_context qs_context;
typedef struct qs_program qs_program;
typedef void (*qs_output_fn)(int64_t value,void *user);
typedef enum qs_status { QS_OK=0, QS_ERROR=1, QS_INVALID_ARGUMENT=2 } qs_status;
typedef enum qs_format { QS_IL, QS_BYTECODE, QS_NATIVE, QS_OBJECT, QS_EXECUTABLE } qs_format;
const char *qs_version(void);
qs_context *qs_create(void);
void qs_destroy(qs_context *);
/* Error and emitted buffers belong to the context until its next operation. */
const char *qs_error(const qs_context *);
void qs_set_output(qs_context *,qs_output_fn,void *user);
void qs_set_step_limit(qs_context *,size_t limit);
qs_program *qs_compile(qs_context *,const char *source,size_t length);
void qs_program_destroy(qs_program *);
qs_status qs_run(qs_context *,const qs_program *,int64_t *result);
/* format is a qs_format code; int permits safely rejecting unknown C values. */
qs_status qs_emit(qs_context *,const qs_program *,int format,const unsigned char **data,size_t *length);
qs_status qs_eval(qs_context *,const char *source,size_t length,int64_t *result);
#ifdef __cplusplus
}

#include "MetaChain.hpp"
#include "metachain/ISched.hpp"
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace qscript {
using metachain::Diagnostic;
template<class T> using Result=metachain::Outcome<T>;
struct AST { dsl::ASTNode root; };
struct Options {
    metachain::Options toolchain{};
    std::size_t max_depth=256,max_nodes=100000;
};
Result<AST> parse(std::string_view source,Options={},std::string file={});
// False from a visitor prunes that node's children. Traversal is pre-order.
bool visit(const AST&,const std::function<bool(const dsl::ASTNode&)>&,std::size_t max_depth=256);
std::vector<Diagnostic> sema_check(const AST&,Options={});
Result<metail::Program> ast_to_ir(const AST&,Options={});
Result<metail::Program> compile(std::string_view,Options={},std::string file={});
Result<metail::Program> optimize(metail::Program,Options={});
Result<metail::Program> schedule(metail::Program,Options={});
Result<std::string> select(const metail::Program&,Options={});
Result<metail::Program> verify_ir(metail::Program,Options={});
Result<std::string> emit(const metail::Program&,Options={});
Result<std::vector<std::uint8_t>> assemble(std::string_view,bool object=true,Options={});
Result<std::vector<std::uint8_t>> link(const std::vector<std::vector<std::uint8_t>>&,Options={});
Result<std::vector<std::uint8_t>> build(const metail::Program&,Options={});
} // namespace qscript
#endif
#endif
