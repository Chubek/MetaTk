#ifndef METACHAIN_QSCRIPT_ANALYSIS_H
#define METACHAIN_QSCRIPT_ANALYSIS_H
// Whole-program analysis: library loading for `use`, declarations (functions
// with their types, constants, grammars), name resolution and constant
// evaluation. Shared by sema_check and the typed lowering (AST2IR.cpp).
#include "qscript/src/Internal.h"
#include "qscript/src/Types.h"
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

struct qs_grammar;

namespace qscript::detail {
inline constexpr std::size_t max_variables = 16;   // r0..r15 per function
inline constexpr unsigned first_temporary = 16;     // r16..r29
inline constexpr unsigned last_temporary = 29;
inline constexpr unsigned return_register = 30;
inline constexpr unsigned scratch_register = 31;
inline constexpr const char* core_unit = "__core";

struct Function {
    std::string name;                 // qualified ("lib::f", or "f" in the program)
    std::string unit;                 // "" for the program itself
    std::vector<std::string> params;
    std::vector<Type> param_types;
    Type result = Type::integer();
    const dsl::ASTNode* decl = nullptr;
    const dsl::ASTNode* body = nullptr;
    std::optional<std::int64_t> host_id;
    std::size_t host_arity = 0;
};
struct Constant {
    std::string name, unit;
    const dsl::ASTNode* decl = nullptr;
    std::optional<std::int64_t> value;
    bool evaluating = false;
};
// A grammar declaration or literal. `source` is the complete text that
// defines it: its parents' declarations followed by its own, so that the
// grammar engine's `is` inheritance resolves inside one source.
struct GrammarDecl {
    std::string name;                 // "Calc", "geom::Calc", or "" for literals
    std::string internal;             // identifier used inside `source`
    std::string unit;                 // declaring unit
    const dsl::ASTNode* decl = nullptr;
    std::string parent;               // as written ("" if none)
    std::string body;                 // text between the braces
    std::string source;
    std::shared_ptr<const qs_grammar> compiled;
    int state = 0;                    // 0 unresolved, 1 resolving, 2 resolved
};
struct UnitInfo {
    std::string name, file;
    std::vector<std::string> imports;
    std::map<std::string, std::string> functions;  // short -> qualified
    std::map<std::string, std::string> constants;  // short -> qualified
    std::map<std::string, std::string> grammars;   // short -> key in Analysis::grammars
};
struct Analysis {
    std::deque<AST> libraries;                       // owns loaded library ASTs
    std::vector<std::string> library_order;
    std::map<std::string, UnitInfo> units;           // "" is the program
    std::map<std::string, Function> functions;
    std::map<std::string, Constant> constants;
    std::map<std::string, GrammarDecl> grammars;     // key: qualified name, or "#n" for literals
    std::map<const dsl::ASTNode*, std::string> grammar_literals;  // grammar-lit node -> key
    std::vector<const dsl::ASTNode*> statements;     // program top-level statements
    std::vector<std::string> emitted;                // user-visible functions to compile
    std::vector<Diagnostic> diagnostics;
    bool uses_power = false;

    // Resolve a name as written in `unit`. Empty on failure, with the reason in `why`.
    std::string function(const std::string& unit, const std::string& name, std::string& why) const;
    std::string constant(const std::string& unit, const std::string& name, std::string& why) const;
    std::string grammar(const std::string& unit, const std::string& name, std::string& why) const;
};
Analysis analyze(const AST&, const Options&);
} // namespace qscript::detail
#endif
