#ifndef METACHAIN_QSCRIPT_ANALYSIS_H
#define METACHAIN_QSCRIPT_ANALYSIS_H
// Whole-program analysis: library loading for `use`, name resolution,
// constant evaluation and semantic checks. Shared by sema_check and lower.
#include "qscript/src/Internal.h"
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

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
    const dsl::ASTNode* decl = nullptr;
    const dsl::ASTNode* body = nullptr;
};
struct Constant {
    std::string name, unit;
    const dsl::ASTNode* decl = nullptr;
    std::optional<std::int64_t> value;
    bool evaluating = false;
};
struct UnitInfo {
    std::string name, file;
    std::vector<std::string> imports;
    std::map<std::string, std::string> functions;  // short -> qualified
    std::map<std::string, std::string> constants;  // short -> qualified
};
struct Analysis {
    std::deque<AST> libraries;                       // owns loaded library ASTs
    std::vector<std::string> library_order;
    std::map<std::string, UnitInfo> units;           // "" is the program
    std::map<std::string, Function> functions;
    std::map<std::string, Constant> constants;
    std::vector<const dsl::ASTNode*> statements;     // program top-level statements
    std::set<std::string> used_functions;            // call graph closure from roots
    std::vector<Diagnostic> diagnostics;
    bool uses_power = false;

    // Resolve a call or name as written in `unit`. Empty on failure, with the
    // reason in `why`.
    std::string function(const std::string& unit, const std::string& name, std::string& why) const;
    std::string constant(const std::string& unit, const std::string& name, std::string& why) const;
};
Analysis analyze(const AST&, const Options&);
} // namespace qscript::detail
#endif
