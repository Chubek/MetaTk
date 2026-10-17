#ifndef METACHAIN_QSCRIPT_RUNTIME_SESSION_H
#define METACHAIN_QSCRIPT_RUNTIME_SESSION_H
// The QScript run-time heap and the built-in operations on strings, lists,
// grammars and matches (not installed API).
//
// Built-ins are reached through MetaIL `host` instructions whose immediate is
// builtin_base + Builtin. The compiler emits them as ordinary calls, so a
// built-in takes its arguments in r0..rN-1 like any function. A Session lives
// for one run (or one call from the host) and owns every handle created in it.
#include "qscript/QScript.h"
#include "qscript/src/Types.h"
#include "qscript/include/QScript-Grammar.h"
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace qscript::detail {
enum class Builtin : std::int64_t {
    // Program-level functions.
    Die, CompileGrammar,
    // Constants and literal handles.
    StrLiteral, GrammarRef, ListNew,
    // List operations (the element type is passed as a type descriptor).
    ListPush, ListGet, ListSet, ListLen, ListPop, ListLast, ListContains, ListSort,
    ListReverse, ListClear, ListJoin, ListSlice,
    // String operations.
    StrCompare, StrConcat, StrLen, StrAt, StrSub, StrFind, StrFindFrom, StrStarts, StrEnds,
    StrSplit, StrRepeat, StrToInt, StrUpper, StrLower, StrTrim, StrContains, StrReplace,
    // Integer methods.
    IntToStr, IntToChar,
    // Grammar methods.
    GrammarParse, GrammarParseRule, GrammarName, GrammarSource, GrammarRules, GrammarExtend, GrammarHasRule,
    // Match methods.
    MatchOk, MatchText, MatchRule, MatchSym, MatchFrom, MatchTo, MatchLine, MatchColumn,
    MatchSize, MatchAt, MatchHas, MatchCount, MatchChild, MatchNth, MatchDump, MatchError, MatchFindAll, MatchChildren,
    // Printing of any value: (value, type descriptor) -> 0.
    PrintValue,
};
inline constexpr std::int64_t builtin_base = std::int64_t(1) << 40;
inline std::int64_t builtin_id(Builtin b) { return builtin_base + static_cast<std::int64_t>(b); }
inline bool is_builtin_id(std::int64_t id) { return id >= builtin_base && id < builtin_base + 256; }
// True when the program calls the built-in runtime (strings, lists, grammars,
// matches). Generated C uses either its standalone runtime or the linked bridge;
// native output uses the small assembly runtime or the statically linked parser.
inline bool uses_builtins(const metail::Program& p) {
    for (const auto& i : p.code) if (i.op == metail::Op::Host && is_builtin_id(i.immediate)) return true;
    return false;
}
// Native output of a bare program has no string or list tables; it needs the unit.
inline constexpr const char* builtins_need_unit = "native output of a program that uses strings, lists, grammars or matches needs its unit; compile the source file";
// The standalone C runtime runs strings/lists. Grammar/match programs link
// QScript-Generated.h's bridge to the existing C++ grammar engine.
// C99 source of the run-time built-ins, placed in generated C programs.
std::string_view c_runtime_source();

// The built-ins that the generated run-times implement (portable C and native
// x86-64). Each name gives its QS_B_<name> number in C and its routine qs_b_<name>
// in assembly, so both run-times stay in step with this one table.
struct RuntimeBuiltin { Builtin id; const char* name; };
inline constexpr RuntimeBuiltin runtime_builtins[] = {
    {Builtin::Die, "Die"}, {Builtin::StrLiteral, "StrLiteral"}, {Builtin::ListNew, "ListNew"},
    {Builtin::ListPush, "ListPush"}, {Builtin::ListGet, "ListGet"}, {Builtin::ListSet, "ListSet"},
    {Builtin::ListLen, "ListLen"}, {Builtin::ListPop, "ListPop"}, {Builtin::ListLast, "ListLast"},
    {Builtin::ListContains, "ListContains"}, {Builtin::ListSort, "ListSort"},
    {Builtin::ListReverse, "ListReverse"}, {Builtin::ListClear, "ListClear"},
    {Builtin::ListJoin, "ListJoin"}, {Builtin::ListSlice, "ListSlice"},
    {Builtin::StrCompare, "StrCompare"}, {Builtin::StrConcat, "StrConcat"}, {Builtin::StrLen, "StrLen"},
    {Builtin::StrAt, "StrAt"}, {Builtin::StrSub, "StrSub"}, {Builtin::StrFind, "StrFind"},
    {Builtin::StrFindFrom, "StrFindFrom"}, {Builtin::StrStarts, "StrStarts"}, {Builtin::StrEnds, "StrEnds"},
    {Builtin::StrContains, "StrContains"}, {Builtin::StrSplit, "StrSplit"}, {Builtin::StrRepeat, "StrRepeat"},
    {Builtin::StrToInt, "StrToInt"}, {Builtin::StrUpper, "StrUpper"}, {Builtin::StrLower, "StrLower"},
    {Builtin::StrTrim, "StrTrim"}, {Builtin::StrReplace, "StrReplace"}, {Builtin::IntToStr, "IntToStr"},
    {Builtin::IntToChar, "IntToChar"}, {Builtin::PrintValue, "PrintValue"},
};
// True for grammar/match operations or nested descriptors. Other programs keep
// their small standalone C/native runtime. Unknown built-ins/types are errors.
Result<bool> needs_full_runtime(const Unit&);
Result<std::string> linked_native_source(const Unit&, const Options&);
bool native_runtime_object(const std::vector<std::uint8_t>&);
Result<std::vector<std::uint8_t>> link_native_runtime(const std::vector<std::vector<std::uint8_t>>&, const Options&);
// One type descriptor as the generated run-times see it: its kind and the index
// of its element descriptor (-1 when it has none).
struct RunTimeType { int kind; int elem; };
// The descriptors of a unit, in the order of Unit::types. Element descriptors
// that a list needs are appended after them.
Result<std::vector<RunTimeType>> run_time_types(const Unit& unit);
// The native run-time for a unit's built-ins (routines, tables and messages),
// for the native backend to place after the program. Sizes are the unit's limits.
std::string native_runtime_text(const std::vector<std::string>& strings, const std::vector<RunTimeType>& types,
                                std::uint64_t max_objects, std::uint64_t max_bytes);

// A resolved built-in call: the operation, the parameter types the arguments
// must have (receiver first for methods), the result type, and whether a
// type descriptor is appended as a final constant argument.
struct BuiltinCall {
    Builtin id;
    std::vector<Type> params;
    Type result;
    std::optional<Type> type_arg;
    std::size_t arity() const { return params.size() + (type_arg ? 1 : 0); }
};
// Method `name` on a receiver of type `recv` with `argc` explicit arguments.
std::optional<BuiltinCall> resolve_method(const Type& recv, std::string_view name, std::size_t argc, std::string& why);
// Global built-in function `name` with `argc` arguments.
std::optional<BuiltinCall> resolve_global(std::string_view name, std::size_t argc);

// One run's heap. Handles are 1-based indices; 0 is never a valid handle.
class Session {
public:
    Session(const Unit& unit, const Options& options);
    // The VM host entry point for builtin ids; regs holds r0..r31.
    std::int64_t dispatch(std::int64_t id, std::span<const std::int64_t> regs);
    // Format a value of type `t` as the text printed by `say`.
    std::string format(std::int64_t value, const Type& t);
    // Printing for a value whose type is known: text out, one line.
    void emit_line(std::string_view text);
    // Integers printed through the session when no text output is installed
    // (they belong to Execution::output then).
    const std::vector<std::int64_t>& printed_integers() const { return ints_; }
    std::int64_t import_value(const Value&);
    Value export_value(std::int64_t, const Type&);

private:
    std::vector<std::int64_t> ints_;
    struct Object {
        Type::Kind kind = Type::Kind::Str;
        std::string text;                          // Str contents; Match matched text
        std::vector<std::int64_t> items;           // List elements
        std::shared_ptr<const qs_grammar> grammar; // Grammar
        std::string name;                          // Grammar display name
        std::string internal;                      // name of the grammar inside `source`
        std::string source;                        // Grammar full source
        std::size_t depth = 0;                     // Grammar derivation depth
        std::shared_ptr<const qs_match> root;      // Match tree root (null on failure)
        const qs_match* node = nullptr;            // Match node (null on failure)
        std::string error;                         // Match failure message
        std::size_t line = 0, column = 0;          // Match failure position
    };
    const Unit& unit_;
    Options options_;                          // a copy: the VM's copy of the options is moved from
    std::vector<Object> heap_;
    std::size_t bytes_ = 0;
    std::vector<std::int64_t> string_handles_;
    std::vector<std::int64_t> grammar_handles_;
    std::map<const HostValue*, std::int64_t> imported_;
    std::map<std::pair<std::int64_t, std::string>, Value> exported_;

    std::int64_t allocate(Object object, std::size_t bytes);
    Object& object(std::int64_t handle, Type::Kind kind);
    const Object& object(std::int64_t handle, Type::Kind kind) const;
    std::int64_t make_string(std::string text);
    std::int64_t make_grammar(std::string name, std::string internal, std::string source, std::size_t depth,
                              std::shared_ptr<const qs_grammar> compiled);
    std::int64_t make_match(std::shared_ptr<const qs_match> root, const qs_match* node, std::string text);
    std::int64_t make_failed_match(std::string error, std::size_t line, std::size_t column);
    std::shared_ptr<const qs_grammar> compile_or_throw(const std::string& source) const;
    std::int64_t parse(std::int64_t grammar, std::int64_t input, const std::string& rule);
    std::int64_t grammar_literal(std::size_t index);
    std::int64_t string_literal(std::size_t index);
    const std::string& text(std::int64_t handle) const { return object(handle, Type::Kind::Str).text; }
    Type type_of(std::int64_t descriptor) const;
    std::int64_t charge_bytes(std::size_t bytes);
    void format_into(std::string& out, std::int64_t value, const Type& t, bool quote);
    static std::int64_t bool_value(bool b) { return b ? 1 : 0; }
};
} // namespace qscript::detail
#endif
