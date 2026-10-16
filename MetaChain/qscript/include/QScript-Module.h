#ifndef METACHAIN_QSCRIPT_MODULE_H
#define METACHAIN_QSCRIPT_MODULE_H
// QScript modules: compile several sources jointly in one shared scope.
//
// A module concatenates the statement lists of its member sources into a
// single program AST, then lowers once. Earlier sources act as a prelude:
// their `let` bindings are visible to later sources, so
// `cat stdlib/prelude.qs program.qs | qscript run -` and the API below agree.
// Member diagnostics keep their own file names. At most 16 simultaneously
// live variables still applies to the merged program. Prelude sources must
// not contain `return` (which halts); they provide bindings, not exits.
// `fn`, `const` and `use` declarations from any member are visible to all.
// A module may also carry named libraries that `use` resolves before the
// context's libraries and the embedded standard library.
#include "qscript/QScript.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qs_module qs_module;

qs_module *qs_module_create(void);
void qs_module_destroy(qs_module *);
qs_status qs_module_add_source(qs_module *, const char *text, size_t length, const char *file);
size_t qs_module_sources(const qs_module *);
qs_status qs_module_compile(qs_context *, const qs_module *, qs_program **program);

/* Read a source file into the module (QS_ERROR if unreadable or too big). */
qs_status qs_module_add_file(qs_module *, const char *path);
/* Provide a library for `use NAME;` to this module only. Module libraries
   take precedence over the context's (QScript-Library.h). */
qs_status qs_module_add_library(qs_module *, const char *name, const char *text, size_t length);
size_t qs_module_library_count(const qs_module *);
/* Remove all sources and libraries. */
void qs_module_clear(qs_module *);

#ifdef __cplusplus
}

#include <map>
#include <string>
#include <vector>

namespace qscript::module {
struct Source {
    std::string text;
    std::string file;
};
struct Module {
    std::vector<Source> sources;
    std::map<std::string, std::string, std::less<>> libraries;
};
inline Result<AST> parse_module(const Module& module, Options options = {}) {
    Result<AST> out;
    if (module.sources.empty()) {
        out.diagnostics.push_back({metachain::Stage::Parse, "module has no sources", {}, 0, 0, 0});
        return out;
    }
    std::vector<dsl::ASTNode> statements;
    for (const auto& s : module.sources) {
        auto parsed = parse(s.text, options, s.file);
        if (!parsed) {
            out.diagnostics.insert(out.diagnostics.end(),
                parsed.diagnostics.begin(), parsed.diagnostics.end());
            return out;
        }
        for (auto& child : parsed->root.children()) statements.push_back(std::move(child));
        if (statements.size() > options.max_nodes) {
            out.diagnostics.push_back({metachain::Stage::Parse, "module AST node limit exceeded", {}, 0, 0, 0});
            return out;
        }
    }
    out.value = AST{dsl::ASTNode("program", std::move(statements))};
    return out;
}
inline Result<Unit> compile_module_unit(const Module& module, Options o = {}, std::string file = {}) {
    for (const auto& [name, text] : module.libraries) o.libraries[name] = text;
    auto ast = parse_module(module, o);
    if (!ast) return {std::nullopt, std::move(ast.diagnostics)};
    auto unit = lower(*ast, o);
    if (!unit) return unit;
    unit->program.name = std::move(file);
    return finish(std::move(*unit), o);
}
inline Result<metail::Program> compile_module(const Module& module, Options o = {}, std::string file = {}) {
    auto unit = compile_module_unit(module, std::move(o), std::move(file));
    if (!unit) return {std::nullopt, std::move(unit.diagnostics)};
    return {std::move(unit->program), {}};
}
} // namespace qscript::module
#endif
#endif
