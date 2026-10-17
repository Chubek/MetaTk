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
/* Text for values that are not integers (str, list, grammar, match), one call per line
   without its newline. Integers always go to the qs_output_fn. */
typedef void (*qs_text_fn)(const char *text,size_t length,void *user);
typedef enum qs_status { QS_OK=0, QS_ERROR=1, QS_INVALID_ARGUMENT=2 } qs_status;
typedef enum qs_format { QS_IL, QS_BYTECODE, QS_NATIVE, QS_OBJECT, QS_EXECUTABLE, QS_C_SOURCE } qs_format;
const char *qs_version(void);
qs_context *qs_create(void);
void qs_destroy(qs_context *);
/* Error and emitted buffers belong to the context until its next operation. */
const char *qs_error(const qs_context *);
void qs_set_output(qs_context *,qs_output_fn,void *user);
void qs_set_text_output(qs_context *,qs_text_fn,void *user);
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
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct qs_grammar;
struct qs_match;

namespace qscript {
using metachain::Diagnostic;
template<class T> using Result=metachain::Outcome<T>;
struct AST { dsl::ASTNode root; };
enum class Scheduler { List, Cycle, Static };
struct BackendOptions {
    Scheduler scheduler=Scheduler::List;
    unsigned issue_width=1;
};
// Resolves `use NAME;` to library source text, or nullopt if unknown.
using LibraryResolver=std::function<std::optional<std::string>(std::string_view name)>;
struct HostFunction {
    std::int64_t id=0;
    std::size_t arity=0;
    std::function<std::int64_t(std::span<const std::int64_t>)> invoke;
};
// Grammar/match native objects link a minimal parser runtime and the static
// C++/C libraries. Empty paths use the CMake-configured defaults. Arguments are
// passed directly to the trusted linker driver, never through a shell.
struct NativeLinkOptions {
    std::string driver, runtime_archive, temporary_directory;
    std::vector<std::string> arguments;
    std::size_t timeout_seconds = 60;
    std::size_t max_diagnostic_bytes = 1024u * 1024u;
};
struct Options {
    metachain::Options toolchain{};
    std::size_t max_depth=256,max_nodes=100000;
    BackendOptions backend{};
    // `use NAME;` looks in, in order: `libraries`, `resolver`, each of
    // `library_paths` (NAME.qs, with :: mapped to /), then the embedded
    // standard library when `stdlib` is true. Nothing touches the filesystem
    // unless library_paths is non-empty.
    std::map<std::string,std::string,std::less<>> libraries{};
    LibraryResolver resolver{};
    std::vector<std::string> library_paths{};
    bool stdlib=true;
    std::size_t max_libraries=256;
    std::map<std::string,HostFunction,std::less<>> hosts;
    bool jit=false;
    // Text printed by `say`/`print` for non-integer values (one line each).
    std::function<void(std::string_view)> text_output{};
    // Limits on the run-time heap (strings, lists, grammars, matches).
    std::size_t max_heap_objects=1'000'000;
    std::size_t max_heap_bytes=256u*1024u*1024u;
    NativeLinkOptions native_link{};
};
// A function callable from the host (qs_call) and its entry point.
struct FunctionInfo {
    std::string name;            // "f" for program functions, "lib::f" for library ones
    std::size_t entry=0, arity=0;
    std::string file;
    std::size_t line=0;
    std::vector<std::string> parameter_types;  // canonical spellings: int, str, list<int>, ...
    std::string result_type="int";
};
// A grammar known to a unit: its name, the complete source that defines it
// (parents first; the last grammar in the source is this one) and the
// compiled form. Units share compiled grammars; the source is kept so that
// run-time derivations (`g.extend(...)`) can extend it.
struct GrammarInfo {
    std::string name;
    std::string source;
    std::shared_ptr<const qs_grammar> compiled;
};
// A compiled program plus the metadata needed to call into it.
struct Unit {
    metail::Program program;
    std::vector<FunctionInfo> functions;
    std::vector<std::string> libraries;   // libraries loaded through `use`
    // Constant tables referenced by the program's instructions.
    std::vector<std::string> strings;     // string literals
    std::vector<std::string> types;       // type descriptors for generic operations
    std::vector<GrammarInfo> grammars;    // grammar declarations and literals
};
namespace detail { struct HostValue; class Session; }
// Immutable, owning host values. Calls copy them into a fresh runtime heap;
// returned values remain valid after that heap, its unit and its context die.
class Value {
    std::shared_ptr<const detail::HostValue> data_;
    explicit Value(std::shared_ptr<const detail::HostValue>);
    friend class detail::Session;
public:
    enum class Kind { Int, Str, List, Grammar, Match };
    Value();
    explicit Value(std::int64_t);
    explicit Value(std::string);
    static Result<Value> list(std::string_view element_type, std::vector<Value>);
    static Result<Value> compile_grammar(std::string_view source, Options = {});
    static Result<Value> from_match(const qs_match&, Options = {});
    Kind kind() const noexcept;
    std::string_view type() const noexcept;
    std::optional<std::int64_t> as_integer() const noexcept;
    const std::string* as_string() const noexcept;
    const std::vector<Value>* as_list() const noexcept;
    const qs_grammar* as_grammar() const noexcept;
    const qs_match* as_match() const noexcept; // null for a failed match
    std::string_view match_error() const noexcept;
};
struct CallResult { Value value; metail::Execution execution; };
Result<AST> parse(std::string_view source,Options={},std::string file={});
// False from a visitor prunes that node's children. Traversal is pre-order.
bool visit(const AST&,const std::function<bool(const dsl::ASTNode&)>&,std::size_t max_depth=256);
std::vector<Diagnostic> sema_check(const AST&,Options={});
Result<metail::Program> ast_to_ir(const AST&,Options={});
Result<Unit> lower(const AST&,Options={});
Result<metail::Program> compile(std::string_view,Options={},std::string file={});
Result<Unit> compile_unit(std::string_view,Options={},std::string file={});
// Optimize and schedule a lowered unit (what compile_unit does after lower).
Result<Unit> finish(Unit,Options={});
Result<metail::Program> optimize(metail::Program,Options={});
Result<metail::Program> schedule(metail::Program,Options={});
Result<std::string> select(const metail::Program&,Options={});
Result<metail::Program> verify_ir(metail::Program,Options={});
Result<std::string> emit(const metail::Program&,Options={});
// C99 source for a unit. Grammar/match programs link the QScript runtime bridge;
// integer/string/list programs remain standalone.
Result<std::string> emit_c(const Unit&,Options={});
Result<std::vector<std::uint8_t>> assemble(std::string_view,bool object=true,Options={});
Result<std::vector<std::uint8_t>> link(const std::vector<std::vector<std::uint8_t>>&,Options={});
// Native output of a unit: strings/lists carry the small assembly runtime;
// grammar/match objects statically link QScriptNativeRuntime and C++/C libraries.
Result<std::string> select(const Unit&,Options={});
Result<std::vector<std::uint8_t>> build(const metail::Program&,Options={});
Result<std::vector<std::uint8_t>> build(const Unit&,Options={});
// Execute a program; trap diagnostics are rewritten to QScript terms
// (e.g. a failed `assert` reports "assertion failed" at its source line).
Result<metail::Execution> run(const metail::Program&,Options={});
// Execute a compiled unit. Programs that use strings, lists or grammars need
// this overload, because their constants live in the unit.
Result<metail::Execution> run(const Unit&,Options={});
// Call one function of a compiled unit with integer arguments.
Result<metail::Execution> call(const Unit&,std::string_view function,const std::vector<std::int64_t>& args,Options={});
// Typed calls support every QScript type without exposing runtime handles.
Result<CallResult> call_value(const Unit&,std::string_view function,std::span<const Value> args,Options={});
// The QScript grammar source (Raku-style; see QScript-Grammar.h).
const char* grammar_source();
// Names and sources of the embedded standard library.
std::vector<std::string> stdlib_names();
std::optional<std::string> stdlib_source(std::string_view name);
} // namespace qscript
#endif
#endif
