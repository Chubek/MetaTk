#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace metapp {

struct Location {
    std::string file = "<input>";
    std::size_t line = 1;
    std::size_t column = 1;
};

class Error : public std::runtime_error {
public:
    Location location;
    Error(Location where, const std::string& message);
};

enum class TokenKind { identifier, number, string_literal, character_literal, punctuation, header_name };

struct Token {
    TokenKind kind = TokenKind::punctuation;
    std::string text;
    Location location;
    bool leading_space = false;
    // Macro names unavailable for further expansion of this token.
    std::set<std::string> hide_set;
};
using Tokens = std::vector<Token>;

Tokens tokenize(std::string_view source, Location start = {});
std::string render(const Tokens& tokens);

struct Macro {
    bool function_like = false;
    bool variadic = false;
    std::vector<std::string> parameters;
    Tokens replacement;
};

struct Options {
    std::vector<std::filesystem::path> include_paths;
    std::size_t max_include_depth = 128;
    std::size_t max_expansions = 100000;
    std::size_t max_expansion_depth = 256;
    std::size_t max_expression_depth = 256;
    std::size_t max_output_tokens = 1000000;
};

struct Diagnostic {
    Location location;
    std::string message;
};

class Preprocessor {
public:
    using Directive = std::function<Tokens(Preprocessor&, const Tokens&, const Location&)>;
    using Kernel = std::function<Tokens(const Tokens&, const Location&)>;

    explicit Preprocessor(Options options = {});
    void define(std::string_view definition); // NAME, NAME=value, or NAME(args)=value
    void undefine(std::string_view name);
    bool is_defined(std::string_view name) const;
    const std::map<std::string, Macro>& macros() const { return macros_; }

    // A preprocessing call resets diagnostics, dependencies, pragma-once state and counters.
    // Macro definitions and registered extensions persist between calls.
    std::string process(std::string_view source, std::string filename = "<input>");
    std::string process_file(const std::filesystem::path& path);
    Tokens expand(const Tokens& tokens);
    void register_directive(std::string name, Directive handler);
    void register_kernel(std::string name, Kernel handler);
    void add_rewrite(Tokens pattern, Tokens replacement);
    void clear_rewrites();

    const std::vector<Diagnostic>& warnings() const { return warnings_; }
    const std::vector<std::filesystem::path>& dependencies() const { return dependencies_; }

private:
    struct Rule { Tokens pattern; Tokens replacement; };
    Options options_;
    std::map<std::string, Macro> macros_;
    std::map<std::string, std::shared_ptr<Directive>> directives_;
    std::map<std::string, std::shared_ptr<Kernel>> kernels_;
    std::vector<Rule> rewrites_;
    std::vector<Diagnostic> warnings_;
    std::vector<std::filesystem::path> dependencies_;
    std::set<std::filesystem::path> once_;
    std::size_t expansions_ = 0;
    std::size_t counter_ = 0;
    std::size_t expansion_depth_ = 0;
    std::size_t emitted_tokens_ = 0;
    bool processing_ = false;

    void reset();
    std::string process_source(std::string_view source, const std::string& filename, std::size_t depth);
    std::string include_file(const std::filesystem::path& path, std::size_t depth);
    std::filesystem::path resolve_include(std::string_view name, bool quoted,
                                         const std::string& current, const Location& location) const;
    void parse_define(const Tokens& tokens, const Location& location);
    bool evaluate(const Tokens& expression, const Location& location);
    Tokens rewrite(Tokens tokens) const;
};

} // namespace metapp
