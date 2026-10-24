#pragma once
#include "../ArchAS.hpp"
#include <optional>

namespace archas::detail {
struct Line { std::string text; Location location; };
using Lines = std::vector<Line>;
using Directives = std::map<std::string, Directive>;
using Constants = std::map<std::string, int64_t>;
struct NonAbsolute : Error { using Error::Error; };

void append(std::string &out, std::string_view text, size_t limit);
std::string directive_name(std::string name);
std::pair<std::string, std::string> head(std::string_view text);
std::vector<std::string> arguments(std::string_view text, bool whitespace = false, bool allow_empty = false);
std::pair<size_t, int64_t> character(std::string_view text, size_t start);
int64_t evaluate(std::string_view text, const Constants &constants);
Lines lex(std::string_view source, const Location &start, bool arm, size_t limit);
Lines raw_lines(std::string_view source, Location start);
Lines lex_lines(const Lines &source, bool arm, size_t limit);
std::string hex(std::string_view data);
std::string json_string(std::string_view data);
std::string image_json(const mair::Image &image, const mair::Target &target);
std::filesystem::path data_root(const std::filesystem::path &explicit_path, bool bfd);

// Each implementation owns its foreign runtime and releases callbacks before
// unloading code. Directives outlive neither Lua nor Plugins.
class Lua {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    Lua(const mair::Target &, const Options &, Directives &);
    ~Lua();
    Lines expand(std::string_view source, const Location &location);
    std::string script(std::string_view source, const std::string &file);
};
class Plugins {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    Plugins(const mair::Target &, const Options &, Directives &);
    ~Plugins();
};

struct Translation {
    Lines lines;
    std::vector<std::filesystem::path> dependencies;
};
Translation preprocess(std::string_view source, const std::string &file,
                       const mair::Target &, const Options &, Directives);
Lines lower(const Lines &, const mair::Target &, const Options &);
mair::Image builtin(const Lines &, const mair::Target &, const Options &);
Output native(const Lines &, const mair::Target &, const Options &);
} // namespace archas::detail
