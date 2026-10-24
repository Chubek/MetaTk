#ifndef METAARCH_ARCHAS_HPP
#define METAARCH_ARCHAS_HPP

#include "../MetaArchIR/MetaArchIR.hpp"
#include <functional>

namespace archas {

using Location = mair::Location;
using Error = mair::Error;
struct IOError : std::runtime_error { using runtime_error::runtime_error; };

enum class Syntax { att, intel };
enum class Backend { automatic, gas, llvm, builtin };
enum class Format { object, assembly, binary, hex, json };

struct Limits {
    size_t bytes = 16 * 1024 * 1024;
    size_t statements = 1000000;
    size_t expansion_depth = 64;
    size_t lua_memory = 64 * 1024 * 1024;
    size_t lua_instructions = 10000000;
};

struct Options {
    Syntax syntax = Syntax::att;
    Backend backend = Backend::automatic;
    Format format = Format::object;
    Limits limits;
    std::vector<std::filesystem::path> include_paths, lua_files, plugins;
    std::map<std::string, int64_t> defines;
    std::filesystem::path assembler; // An executable, never a shell command.
    std::vector<std::string> assembler_args;
    std::string triple; // Explicit override for a native assembler target.
    std::filesystem::path bfd_root; // Otherwise ARCHBFDDB / installed / source data.
};

struct DirectiveContext {
    const mair::Target &target; // Borrowed for the duration of the callback.
    const Location &location;
    Syntax syntax;
};
// Return assembly/MetaArchIR source to recursively expand at the invocation site.
// Throw Error on failure. Arguments are borrowed, the returned string is owned.
using Directive = std::function<std::string(std::string_view, const DirectiveContext &)>;

struct Output {
    std::string data; // Binary for object/binary formats; otherwise UTF-8/ASCII text.
    std::string diagnostics; // Native assembler warnings, with source locations.
    std::vector<std::filesystem::path> dependencies;
};

// Loads a validated .march or Dsymspec file when spec_file is supplied. Otherwise
// searches db, METAARCHDB, the installed database and the source database.
// Returns a shared immutable target; throws Error on invalid target/specification.
mair::Target load_target(std::string name, std::filesystem::path db = {},
                         size_t index = 0, std::filesystem::path spec_file = {});
std::vector<std::string> targets(std::filesystem::path db = {});
bool has_lua() noexcept;
bool has_plugins() noexcept;

// Owns its target, options and callbacks. compile() creates a fresh translation
// unit (Lua VM, macro table and plugin instances) on every call. Const calls may
// run concurrently if user callbacks/plugins are thread-safe. Mutators require
// exclusive access. All results own their storage. Errors throw Error, file and
// process failures throw IOError; allocation may throw std::bad_alloc.
class Assembler {
    mair::Target target_;
    Options options_;
    std::map<std::string, Directive> directives_;
public:
    explicit Assembler(mair::Target target, Options options = {});
    // Names have an optional leading dot; built-ins and duplicates are errors.
    Assembler &directive(std::string name, Directive callback);
    Output compile(std::string_view source, const std::string &file = "<memory>") const;
};

// Bounded binary input, and atomic same-directory output replacement. Neither
// retains borrowed memory. IO failures throw IOError. The CLI checks aliases to
// all translation-unit dependencies before calling write_file().
std::string read_file(const std::filesystem::path &path, size_t limit);
void write_file(const std::filesystem::path &path, std::string_view data);

} // namespace archas
#endif
