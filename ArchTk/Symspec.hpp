/**
 * @file Symspec.hpp
 * @brief Header-only C++20 access to DomSymAsCC architecture specifications.
 *
 * Link Boost.JSON (-lboost_json). Lua is needed only when evaluating the
 * original .dsymspec DSL; generated backend Lua modules and JSON load directly.
 * DSL evaluation is delegated to the existing Lua/DSL runtime or a host
 * callback. This header does not implement another Lua grammar/interpreter.
 *
 * Example:
 * @code
 * symspec::Repository repo("stdspec");
 * auto spec = symspec::Spec::load("stdspec/msp430.dsymspec",
 *     {.lua = {.dsl_directory = "path/to/domsymas/compiler/stdspec"}});
 * // Faster: load the generated backend directly, no Lua needed.
 * auto backend = repo.load("msp430"); // stdspec/backend/msp430.lua
 * auto add = backend->record("ADD16rr");
 * auto members = backend->register_members("GR16");
 * backend->save("msp430.json");
 * backend->feed([](std::string_view json) { send_to_app(json); return symspec::Status{std::monostate{}}; });
 * @endcode
 *
 * Recoverable failures return Result<T>. In C++23 it is std::expected<T,
 * Error>; C++20 uses an expected-style compatibility type. No public API
 * deliberately throws. Standard allocation failures and exceptions raised
 * by host callbacks follow the host's normal C++ policy.
 *
 * RecordView and pointers returned by queries borrow the Spec. They remain
 * valid until that Spec is destroyed, assigned to, or moved from. Copying a
 * Spec deep-copies its JSON data. Unknown fields and JSON null are preserved.
 */
#pragma once

#include <boost/json.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <istream>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>
#if __has_include(<expected>)
#include <expected>
#endif
#if defined(__unix__) || defined(__APPLE__)
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace symspec {

using Json = boost::json::value;
using Path = std::filesystem::path;

enum class ErrorCode { io, parse, schema, missing, unsupported, evaluation };

struct Error {
    ErrorCode code = ErrorCode::parse;
    std::string message;
    Path path;
    std::string record;
    std::optional<std::size_t> offset;
};

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
template<class T> using Result = std::expected<T, Error>;
using Unexpected = std::unexpected<Error>;
#else
struct Unexpected { Error error; };
template<class T> class Result {
    std::variant<T, Error> data_;
public:
    Result(T value) : data_(std::in_place_index<0>, std::move(value)) {}
    Result(Unexpected error) : data_(std::in_place_index<1>, std::move(error.error)) {}
    bool has_value() const noexcept { return data_.index() == 0; }
    explicit operator bool() const noexcept { return has_value(); }
    T& value() & { assert(has_value()); return *std::get_if<0>(&data_); }
    const T& value() const& { assert(has_value()); return *std::get_if<0>(&data_); }
    T&& value() && { assert(has_value()); return std::move(*std::get_if<0>(&data_)); }
    Error& error() & { assert(!has_value()); return *std::get_if<1>(&data_); }
    const Error& error() const& { assert(!has_value()); return *std::get_if<1>(&data_); }
    T& operator*() & { return value(); }
    const T& operator*() const& { return value(); }
    T&& operator*() && { return std::move(*this).value(); }
    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }
};
#endif

using Status = Result<std::monostate>;

inline Unexpected fail(ErrorCode code, std::string message, Path path = {},
                       std::string record = {}) {
    return Unexpected{Error{code, std::move(message), std::move(path), std::move(record), {}}};
}

inline boost::json::string_view json_key(std::string_view s) { return {s.data(), s.size()}; }
inline const Json* field(const Json& value, std::string_view name) noexcept {
    const auto* object = value.if_object();
    return object ? object->if_contains(json_key(name)) : nullptr;
}
inline std::optional<std::string_view> text(const Json* value) noexcept {
    if (!value || !value->is_string()) return {};
    const auto& s = value->as_string();
    return std::string_view{s.data(), s.size()};
}
inline std::optional<std::int64_t> integer(const Json* value) noexcept {
    if (!value) return {};
    if (value->is_int64()) return value->as_int64();
    if (value->is_uint64() && value->as_uint64() <= std::uint64_t(INT64_MAX))
        return static_cast<std::int64_t>(value->as_uint64());
    // DomMune may emit integral values as 1.0. Do not narrow out-of-range doubles.
    if (value->is_double()) {
        const double n = value->as_double();
        if (std::isfinite(n) && std::trunc(n) == n && n >= -0x1p63 && n < 0x1p63)
            return static_cast<std::int64_t>(n);
    }
    return {};
}
inline Result<std::optional<bool>> flag(const Json* value) {
    if (!value || value->is_null()) return std::optional<bool>{};
    if (value->is_bool()) return std::optional<bool>{value->as_bool()};
    if (auto n = integer(value); n && (*n == 0 || *n == 1))
        return std::optional<bool>{*n != 0};
    return fail(ErrorCode::schema, "expected boolean, 0, 1 or null");
}

enum class Group { allocation, selection, scheduling, instructions, dependencies };
inline constexpr std::array<Group, 5> record_groups = {
    Group::allocation, Group::selection, Group::scheduling, Group::instructions, Group::dependencies
};
inline constexpr std::string_view group_name(Group group) noexcept {
    switch (group) {
    case Group::allocation: return "register_allocation";
    case Group::selection: return "instruction_selection";
    case Group::scheduling: return "instruction_scheduling";
    case Group::instructions: return "instructions";
    case Group::dependencies: return "dependencies";
    }
    return {};
}

struct RecordView {
    std::string_view id;
    const Json* value = nullptr;
    const Json* get(std::string_view name) const noexcept { return value ? field(*value, name) : nullptr; }
    bool has_class(std::string_view name) const noexcept {
        auto* classes = get("classes");
        if (!classes || !classes->is_array()) return false;
        return std::any_of(classes->as_array().begin(), classes->as_array().end(),
                           [&](const Json& c) { return text(&c) == name; });
    }
};

struct ParseOptions {
    std::size_t max_bytes = 64 * 1024 * 1024;
    std::size_t max_depth = 256;
    bool check_references = true;
};

namespace detail {

inline bool whitespace(char c) noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

inline Result<std::string> read(std::istream& input, std::size_t limit, Path path = {}) {
    if (!input.good()) return fail(ErrorCode::io, "input stream is not readable", path);
    std::string result;
    std::array<char, 8192> buffer{};
    while (input.read(buffer.data(), buffer.size()) || input.gcount()) {
        const auto n = static_cast<std::size_t>(input.gcount());
        if (n > limit - result.size()) return fail(ErrorCode::io, "input exceeds max_bytes", path);
        result.append(buffer.data(), n);
    }
    if (input.bad() || (!input.eof() && input.fail())) return fail(ErrorCode::io, "reading input failed", path);
    return result;
}

inline Result<std::string> read_file(const Path& path, std::size_t limit) {
    std::ifstream input(path, std::ios::binary);
    return read(input, limit, path);
}

inline std::vector<std::string> references(const Json& value) {
    std::vector<std::string> result;
    std::vector<const Json*> pending{&value};
    std::set<std::string> seen;
    while (!pending.empty()) {
        auto* node = pending.back();
        pending.pop_back();
        if (node->is_object()) {
            if (text(field(*node, "kind")) == "def") {
                if (auto id = text(field(*node, "def")); id && seen.emplace(*id).second)
                    result.emplace_back(*id);
            }
            for (const auto& pair : node->as_object()) pending.push_back(&pair.value());
        } else if (node->is_array()) {
            for (const auto& element : node->as_array()) pending.push_back(&element);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

inline Result<std::string_view> unwrap_module(std::string_view source) {
    // Recognize the generated data envelope only, not arbitrary Lua syntax.
    std::size_t p = 0;
    const auto skip = [&] {
        while (p < source.size()) {
            if (whitespace(source[p])) { ++p; continue; }
            if (source.substr(p, 2) == "--") {
                auto end = source.find('\n', p);
                p = end == source.npos ? source.size() : end + 1;
                continue;
            }
            break;
        }
    };
    skip();
    if (source.substr(p, 6) != "return")
        return fail(ErrorCode::parse, "expected a generated Lua backend data module");
    p += 6;
    while (p < source.size() && whitespace(source[p])) ++p;
    const auto start = p;
    if (p == source.size() || source[p++] != '[')
        return fail(ErrorCode::parse, "expected a Lua long string after return");
    while (p < source.size() && source[p] == '=') ++p;
    if (p == source.size() || source[p++] != '[')
        return fail(ErrorCode::parse, "invalid Lua long-string delimiter");
    const std::string closing = "]" + std::string(source.substr(start + 1, p - start - 2)) + "]";
    const auto end = source.find(closing, p);
    if (end == source.npos) return fail(ErrorCode::parse, "unterminated Lua data module");
    auto contents = source.substr(p, end - p);
    p = end + closing.size();
    if (p < source.size() && source[p] == ';') ++p;
    skip();
    if (p != source.size()) return fail(ErrorCode::parse, "unexpected code after Lua data module");
    return contents;
}

inline std::string long_string(std::string_view data) {
    std::string equals;
    while (data.find("]" + equals + "]") != data.npos) equals += '=';
    return "[" + equals + "[" + std::string(data) + "]" + equals + "]";
}

inline Status write_json(std::ostream& output, const Json& value) {
    boost::json::serializer serializer;
    serializer.reset(&value);
    std::array<char, 8192> buffer{};
    while (!serializer.done()) {
        auto chunk = serializer.read(buffer.data(), buffer.size());
        output.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        if (!output) return fail(ErrorCode::io, "writing JSON failed");
    }
    return std::monostate{};
}

} // namespace detail

/** Host evaluators receive the original source and its file/module origin. */
using Evaluator = std::function<Result<std::string>(std::string_view, const Path&)>;

struct LuaRuntime {
    std::string executable = "lua";
    Path dsl_directory;
    std::vector<Path> module_directories;
    std::size_t max_output_bytes = 64 * 1024 * 1024;

    Result<std::string> evaluate(std::string_view source, const Path& origin) const {
#if defined(__unix__) || defined(__APPLE__)
        std::error_code ec;
        auto directory = std::filesystem::absolute(origin.empty() ? Path{"."} : origin.parent_path(), ec);
        if (ec) return fail(ErrorCode::io, ec.message(), origin);
        std::string search = (directory / "?.lua").string();
        const auto add_directory = [&](const Path& path) -> Status {
            auto absolute = std::filesystem::absolute(path, ec);
            if (ec) return fail(ErrorCode::io, ec.message(), path);
            search += ";" + (absolute / "?.lua").string();
            return std::monostate{};
        };
        if (!dsl_directory.empty()) { auto r = add_directory(dsl_directory); if (!r) return Unexpected{r.error()}; }
        for (const auto& path : module_directories) {
            auto r = add_directory(path); if (!r) return Unexpected{r.error()};
        }
        using File = std::unique_ptr<std::FILE, int(*)(std::FILE*)>;
        File input(std::tmpfile(), std::fclose), output(std::tmpfile(), std::fclose), errors(std::tmpfile(), std::fclose);
        if (!input || !output || !errors) return fail(ErrorCode::io, "cannot create Lua runtime streams", origin);
        constexpr std::string_view prelude = "package.path = arg[1] .. ';' .. package.path\n";
        if (std::fwrite(prelude.data(), 1, prelude.size(), input.get()) != prelude.size() ||
            std::fwrite(source.data(), 1, source.size(), input.get()) != source.size() ||
            std::fflush(input.get()) || std::fseek(input.get(), 0, SEEK_SET))
            return fail(ErrorCode::io, "cannot write Lua source", origin);
        std::vector<std::string> arguments{executable, "-", search};
        std::vector<char*> argv;
        for (auto& argument : arguments) argv.push_back(argument.data());
        argv.push_back(nullptr);
        posix_spawn_file_actions_t actions;
        int error = posix_spawn_file_actions_init(&actions);
        if (error) return fail(ErrorCode::evaluation, std::strerror(error), origin);
        struct ActionsGuard {
            posix_spawn_file_actions_t* actions;
            ~ActionsGuard() { posix_spawn_file_actions_destroy(actions); }
        } guard{&actions};
        const std::array<std::pair<int, int>, 3> descriptors = {{{fileno(input.get()), STDIN_FILENO},
            {fileno(output.get()), STDOUT_FILENO}, {fileno(errors.get()), STDERR_FILENO}}};
        for (auto [from, to] : descriptors) {
            error = posix_spawn_file_actions_adddup2(&actions, from, to);
            if (!error && from > STDERR_FILENO) error = posix_spawn_file_actions_addclose(&actions, from);
            if (error) return fail(ErrorCode::evaluation, std::strerror(error), origin);
        }
        pid_t child{};
        error = posix_spawnp(&child, executable.c_str(), &actions, nullptr, argv.data(), ::environ);
        if (error) return fail(ErrorCode::evaluation, std::strerror(error), origin);
        int status{};
        while (waitpid(child, &status, 0) < 0) {
            if (errno != EINTR) return fail(ErrorCode::evaluation, std::strerror(errno), origin);
        }
        const auto capture = [&](std::FILE* file, std::size_t limit) -> Result<std::string> {
            if (std::fseek(file, 0, SEEK_SET)) return fail(ErrorCode::io, "cannot seek Lua output", origin);
            std::string result;
            std::array<char, 8192> buffer{};
            while (auto n = std::fread(buffer.data(), 1, buffer.size(), file)) {
                if (n > limit - result.size()) return fail(ErrorCode::evaluation, "Lua output exceeds max_output_bytes", origin);
                result.append(buffer.data(), n);
            }
            if (std::ferror(file)) return fail(ErrorCode::io, "cannot read Lua output", origin);
            return result;
        };
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            auto diagnostic = capture(errors.get(), max_output_bytes);
            return fail(ErrorCode::evaluation, "Lua evaluation failed: " +
                (diagnostic ? *diagnostic : diagnostic.error().message), origin);
        }
        return capture(output.get(), max_output_bytes);
#else
        (void)source;
        return fail(ErrorCode::unsupported, "provide a host Evaluator on this platform", origin);
#endif
    }
};

enum class InputFormat { automatic, json, backend_module, lua_dsl };
enum class OutputFormat { json, backend_json, backend_module, lua_document };
struct LoadOptions {
    ParseOptions parse;
    InputFormat format = InputFormat::automatic;
    Evaluator evaluator;
    LuaRuntime lua;
};

class Spec {
    Json document_;
    bool backend_only_ = false;
    explicit Spec(Json value, bool backend_only) : document_(std::move(value)), backend_only_(backend_only) {}

    Status validate(bool check_references) const {
        if (!document_.is_object() || integer(field(document_, "schema")) != 1)
            return fail(ErrorCode::schema, "expected object with schema = 1");
        const auto validate_names = [&](std::string_view key) -> Status {
            const auto* value = field(document_, key);
            if (!value) return std::monostate{};
            if (!value->is_array()) return fail(ErrorCode::schema, std::string(key) + " must be an array");
            std::set<std::string> names;
            for (const auto& item : value->as_array()) {
                if (!item.is_object()) return fail(ErrorCode::schema, std::string(key) + " entries must be objects");
                if (key != "forms" && key != "debug" && key != "targets") {
                    auto name = text(field(item, "name"));
                    if (!name || name->empty() || !names.emplace(*name).second)
                        return fail(ErrorCode::schema, "missing or duplicate name in " + std::string(key));
                }
            }
            return std::monostate{};
        };
        if (!backend_only_) {
            constexpr std::array<std::string_view, 12> arrays = {"targets", "features", "registers", "mnemonics",
                "encoders", "forms", "syntaxes", "sections", "linkages", "relocs", "debug", "operands"};
            for (auto key : arrays) { auto r = validate_names(key); if (!r) return r; }
            const auto* targets = field(document_, "targets");
            if (!targets || !targets->is_array() || targets->as_array().size() != 1 ||
                !text(field(targets->as_array().front(), "arch")))
                return fail(ErrorCode::schema, "a Spec requires exactly one target with an arch name");
            const auto contains = [&](std::string_view array, std::string_view name) {
                const auto* values = field(document_, array);
                return values && values->is_array() && std::any_of(values->as_array().begin(), values->as_array().end(),
                    [&](const Json& v) { return text(field(v, "name")) == name; });
            };
            const auto check_list = [&](const Json& record, std::string_view key, std::string_view table) -> Status {
                auto* list = field(record, key);
                if (!list) return std::monostate{};
                if (!list->is_array()) return fail(ErrorCode::schema, std::string(key) + " must be an array");
                for (const auto& element : list->as_array()) {
                    auto name = text(&element);
                    if (!name || !contains(table, *name))
                        return fail(ErrorCode::schema, "unknown reference in " + std::string(key));
                }
                return std::monostate{};
            };
            auto r = check_list(targets->as_array().front(), "features", "features"); if (!r) return r;
            if (auto* registers = field(document_, "registers")) for (const auto& reg : registers->as_array()) {
                if (auto* parent = field(reg, "parent"); parent && !parent->is_null()) {
                    auto name = text(parent);
                    if (!name || !contains("registers", *name)) return fail(ErrorCode::schema, "unknown register parent");
                }
            }
            if (auto* forms = field(document_, "forms")) for (const auto& form : forms->as_array()) {
                for (auto [key, table] : {std::pair{"mnemonic", "mnemonics"}, std::pair{"encoder", "encoders"}}) {
                    auto name = text(field(form, key));
                    if (!name || !contains(table, *name)) return fail(ErrorCode::schema, "unknown form " + std::string(key));
                }
                for (auto [key, table] : {std::pair{"required", "features"}, std::pair{"forbidden", "features"},
                                         std::pair{"syntaxes", "syntaxes"}}) {
                    auto checked = check_list(form, key, table); if (!checked) return checked;
                }
                if (auto* operands = field(form, "operands"); operands && !operands->is_array())
                    return fail(ErrorCode::schema, "form operands must be an array");
            }
        }
        const auto* b = backend();
        if (!b) return std::monostate{}; // Legacy assembler-only documents.
        if (!b->is_object() || integer(field(*b, "schema")) != 1 ||
            text(field(*b, "arch")) != architecture())
            return fail(ErrorCode::schema, "invalid backend schema or architecture");
        for (auto group : record_groups) {
            auto* map = field(*b, group_name(group));
            if (!map || !map->is_object()) return fail(ErrorCode::schema, "missing backend record map: " + std::string(group_name(group)));
            for (const auto& pair : map->as_object()) {
                if (!pair.value().is_object()) return fail(ErrorCode::schema, "backend records must be objects");
                if (check_references) for (const auto& id : detail::references(pair.value())) {
                    if (!find_record(id)) return fail(ErrorCode::schema, "unresolved backend reference: " + id, {}, std::string(pair.key()));
                }
            }
        }
        return std::monostate{};
    }

public:
    static Result<Spec> from_value(Json value, ParseOptions options = {}) {
        const bool backend_only = field(value, "arch") && !field(value, "targets");
        Spec spec(std::move(value), backend_only);
        auto checked = spec.validate(options.check_references);
        if (!checked) return Unexpected{checked.error()};
        return spec;
    }

    static Result<Spec> from_json(std::string_view input, ParseOptions options = {}) {
        if (input.size() > options.max_bytes) return fail(ErrorCode::parse, "JSON exceeds max_bytes");
        boost::system::error_code ec;
        boost::json::parse_options parse;
        parse.max_depth = options.max_depth;
        Json document = boost::json::parse(json_key(input), ec, {}, parse);
        if (ec) return fail(ErrorCode::parse, ec.message());
        return from_value(std::move(document), options);
    }

    static Result<Spec> from_bytes(std::span<const std::byte> input, ParseOptions options = {}) {
        return from_json(std::string_view(reinterpret_cast<const char*>(input.data()), input.size()), options);
    }

    static Result<Spec> parse(std::string_view input, InputFormat format = InputFormat::json,
                              const LoadOptions& options = {}, const Path& origin = {}) {
        if (input.size() > options.parse.max_bytes) return fail(ErrorCode::parse, "input exceeds max_bytes", origin);
        if (format == InputFormat::automatic) {
            if (origin.extension() == ".dsymspec") format = InputFormat::lua_dsl;
            else if (origin.extension() == ".lua") format = InputFormat::backend_module;
            else format = InputFormat::json;
        }
        Result<Spec> result = [&]() -> Result<Spec> {
            if (format == InputFormat::json) return from_json(input, options.parse);
            if (format == InputFormat::backend_module) {
                auto data = detail::unwrap_module(input);
                if (!data) return Unexpected{data.error()};
                auto parsed = from_json(*data, options.parse);
                if (parsed && !parsed->backend_only()) return fail(ErrorCode::schema, "backend module must contain backend metadata");
                return parsed;
            }
            auto output = options.evaluator ? options.evaluator(input, origin) : options.lua.evaluate(input, origin);
            if (!output) return Unexpected{output.error()};
            return from_json(*output, options.parse);
        }();
        if (!result && result.error().path.empty()) result.error().path = origin;
        return result;
    }

    static Result<Spec> load(const Path& path, const LoadOptions& options = {}) {
        auto source = detail::read_file(path, options.parse.max_bytes);
        if (!source) return Unexpected{source.error()};
        return parse(*source, options.format, options, path);
    }

    bool backend_only() const noexcept { return backend_only_; }
    const Json& json() const noexcept { return document_; }
    const Json* backend() const noexcept { return backend_only_ ? &document_ : field(document_, "backend"); }
    bool has_backend() const noexcept { return backend() != nullptr; }
    std::string_view architecture() const noexcept {
        if (backend_only_) return text(field(document_, "arch")).value_or(std::string_view{});
        auto* targets = field(document_, "targets");
        if (!targets || !targets->is_array() || targets->as_array().empty()) return {};
        return text(field(targets->as_array().front(), "arch")).value_or(std::string_view{});
    }

    const boost::json::array* assembler(std::string_view key) const noexcept {
        const Json* value = backend_only_ ? nullptr : field(document_, key);
        return value ? value->if_array() : nullptr;
    }
    const Json* target() const noexcept {
        auto* targets = assembler("targets");
        return targets && !targets->empty() ? &targets->front() : nullptr;
    }
    const Json* backend_section(std::string_view key) const noexcept {
        const auto* b = backend();
        return b ? field(*b, key) : nullptr;
    }
    const Json* sleigh() const noexcept { return backend_section("sleigh"); }
    const Json* compiler_policy() const noexcept { return backend_section("compiler_policy"); }
    const Json* provenance() const noexcept { return backend_section("provenance"); }
    const Json* coverage() const noexcept { return backend_section("coverage"); }

    std::vector<const Json*> assembler_forms(std::string_view mnemonic) const {
        std::vector<const Json*> result;
        if (auto* forms = assembler("forms")) for (const auto& form : *forms) {
            if (text(field(form, "mnemonic")) == mnemonic) result.push_back(&form);
        }
        return result;
    }
    const Json* assembler_register(std::string_view name) const noexcept {
        if (auto* registers = assembler("registers")) for (const auto& reg : *registers) {
            if (text(field(reg, "name")) == name) return &reg;
        }
        return nullptr;
    }

    const boost::json::object* records(Group group) const noexcept {
        const auto* b = backend();
        const auto* map = b ? field(*b, group_name(group)) : nullptr;
        return map ? map->if_object() : nullptr;
    }

    std::optional<RecordView> find_record(std::string_view id) const noexcept {
        for (auto group : record_groups) if (auto* map = records(group)) {
            auto it = map->find(json_key(id));
            if (it != map->end()) return RecordView{std::string_view(it->key().data(), it->key().size()), &it->value()};
        }
        return {};
    }
    Result<RecordView> record(std::string_view id) const {
        if (auto found = find_record(id)) return *found;
        return fail(ErrorCode::missing, "backend record not found: " + std::string(id), {}, std::string(id));
    }

    std::vector<RecordView> records_with_class(Group group, std::string_view name) const {
        std::vector<RecordView> result;
        if (auto* map = records(group)) for (const auto& entry : *map) {
            RecordView view{std::string_view(entry.key().data(), entry.key().size()), &entry.value()};
            if (view.has_class(name)) result.push_back(view);
        }
        return result;
    }

    std::vector<RecordView> instructions_for(std::string_view mnemonic) const {
        const auto normalize = [](std::string_view name) {
            std::string result;
            for (char c : name) {
                if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
                    !(c >= '0' && c <= '9') && c != '_' && c != '.') break;
                result.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c);
            }
            return result;
        };
        const auto requested = normalize(mnemonic);
        std::vector<RecordView> result;
        if (requested.empty()) return result;
        if (auto* map = records(Group::instructions)) for (const auto& entry : *map) {
            auto assembly = text(field(entry.value(), "AsmString"));
            if (assembly && normalize(*assembly) == requested)
                result.push_back({std::string_view(entry.key().data(), entry.key().size()), &entry.value()});
        }
        return result;
    }

    Result<std::vector<RecordView>> dependency_closure(std::string_view id) const {
        auto root = record(id);
        if (!root) return Unexpected{root.error()};
        std::vector<RecordView> result;
        std::vector<RecordView> pending{*root};
        std::set<std::string_view> seen;
        while (!pending.empty()) {
            auto next = pending.back(); pending.pop_back();
            if (!seen.insert(next.id).second) continue;
            result.push_back(next);
            for (const auto& reference : detail::references(*next.value)) {
                auto resolved = record(reference);
                if (!resolved) return Unexpected{resolved.error()};
                pending.push_back(*resolved);
            }
        }
        return result;
    }

    /** Expand add/sub/and/sequence register-set DAGs, preserving allocation order.
     * These are declared members, NOT a function's allocatable-register mask.
     * Unsupported tuple/custom operators return an error rather than guesses.
     */
    Result<std::vector<std::string>> register_members(std::string_view class_id) const {
        auto cls = record(class_id);
        if (!cls) return Unexpected{cls.error()};
        if (!cls->has_class("RegisterClass")) return fail(ErrorCode::schema, "record is not a RegisterClass", {}, std::string(class_id));
        std::set<std::string> active;
        std::function<Result<std::vector<std::string>>(const Json&, std::size_t)> expand;
        expand = [&](const Json& node, std::size_t depth) -> Result<std::vector<std::string>> {
            if (depth > 64) return fail(ErrorCode::schema, "recursive/deep register-set expression");
            if (text(field(node, "kind")) == "def") {
                auto id = text(field(node, "def"));
                if (!id) return fail(ErrorCode::schema, "register-set reference lacks def");
                auto item = record(*id); if (!item) return Unexpected{item.error()};
                if (item->has_class("Register")) return std::vector<std::string>{std::string(*id)};
                if (auto* members = item->get("MemberList")) {
                    if (!active.emplace(*id).second) return fail(ErrorCode::schema, "cyclic register class");
                    auto expanded = expand(*members, depth + 1); active.erase(std::string(*id)); return expanded;
                }
                return fail(ErrorCode::unsupported, "register tuples/custom set reference: " + std::string(*id));
            }
            auto* op = field(node, "operator");
            auto name = op ? text(field(*op, "def")) : std::nullopt;
            auto* arguments = field(node, "args");
            if (!name || !arguments || !arguments->is_array()) return fail(ErrorCode::schema, "invalid register-set DAG");
            const auto& args = arguments->as_array();
            const auto argument = [&](std::size_t i) -> const Json* {
                if (i >= args.size() || !args[i].is_array() || args[i].as_array().size() != 2) return nullptr;
                return &args[i].as_array()[0];
            };
            if (*name == "sequence") {
                auto pattern = text(argument(0)); auto first = integer(argument(1)); auto last = integer(argument(2));
                if (args.size() != 3 || !pattern || !first || !last || *first < 0 || *last < *first || *last - *first > 65536)
                    return fail(ErrorCode::schema, "invalid register sequence");
                auto placeholder = pattern->find('%');
                if (placeholder == pattern->npos || placeholder + 1 == pattern->size() ||
                    std::string_view("udi").find((*pattern)[placeholder + 1]) == std::string_view::npos)
                    return fail(ErrorCode::unsupported, "unsupported sequence name template");
                std::vector<std::string> result;
                for (auto number = *first; number <= *last; ++number) {
                    auto id = std::string(pattern->substr(0, placeholder)) + std::to_string(number) + std::string(pattern->substr(placeholder + 2));
                    auto item = record(id);
                    if (!item || !item->has_class("Register")) return fail(ErrorCode::schema, "sequence names an unknown register: " + id);
                    result.push_back(std::move(id));
                    if (number == *last) break;
                }
                return result;
            }
            if (*name != "add" && *name != "sub" && *name != "and")
                return fail(ErrorCode::unsupported, "unsupported register-set operator: " + std::string(*name));
            std::vector<std::string> result;
            for (std::size_t i = 0; i < args.size(); ++i) {
                auto* arg = argument(i); if (!arg) return fail(ErrorCode::schema, "invalid register-set argument");
                auto part = expand(*arg, depth + 1); if (!part) return Unexpected{part.error()};
                if (i == 0 || *name == "add") {
                    for (auto& id : *part) if (std::find(result.begin(), result.end(), id) == result.end()) result.push_back(id);
                } else {
                    std::erase_if(result, [&](const auto& id) {
                        const bool found = std::find(part->begin(), part->end(), id) != part->end();
                        return *name == "sub" ? found : !found;
                    });
                }
            }
            return result;
        };
        auto* members = cls->get("MemberList");
        if (!members) return fail(ErrorCode::schema, "RegisterClass lacks MemberList");
        return expand(*members, 0);
    }

    Result<std::vector<std::string>> reserved_registers(std::span<const std::string_view> conditions = {}) const {
        auto* b = backend(); auto* policy = b ? field(*b, "compiler_policy") : nullptr;
        auto* allocation = policy ? field(*policy, "allocation") : nullptr;
        if (!allocation) return fail(ErrorCode::unsupported, "no reserved-register policy for this architecture");
        std::vector<std::string> result;
        const auto append = [&](const Json* registers) -> Status {
            if (!registers || !registers->is_array()) return fail(ErrorCode::schema, "reserved registers must be an array");
            for (const auto& entry : registers->as_array()) {
                auto name = text(&entry);
                if (!name || !find_record(*name)) return fail(ErrorCode::schema, "unknown reserved register");
                if (std::find(result.begin(), result.end(), *name) == result.end()) result.emplace_back(*name);
            }
            return std::monostate{};
        };
        auto fixed = append(field(*allocation, "reserved")); if (!fixed) return Unexpected{fixed.error()};
        if (auto* conditional = field(*allocation, "conditional_reserved")) {
            if (!conditional->is_array()) return fail(ErrorCode::schema, "conditional_reserved must be an array");
            for (const auto& rule : conditional->as_array()) {
                auto when = text(field(rule, "when"));
                if (!when) return fail(ErrorCode::schema, "conditional reservation lacks when");
                if (std::find(conditions.begin(), conditions.end(), *when) != conditions.end()) {
                    auto added = append(field(rule, "registers")); if (!added) return Unexpected{added.error()};
                }
            }
        }
        return result;
    }

    template<class F> requires std::invocable<F, RecordView>
    void visit(Group group, F&& visitor) const {
        if (auto* map = records(group)) for (const auto& entry : *map)
            std::invoke(visitor, RecordView{std::string_view(entry.key().data(), entry.key().size()), &entry.value()});
    }

    /** Edit a deep copy, validate it, then return a new Spec. */
    template<class F> requires std::same_as<std::invoke_result_t<F, Json&>, Status>
    Result<Spec> edited(F&& edit, ParseOptions options = {}) const {
        Json copy = document_;
        auto status = std::invoke(std::forward<F>(edit), copy);
        if (!status) return Unexpected{status.error()};
        return from_value(std::move(copy), options);
    }

    Result<std::string> serialize(OutputFormat format = OutputFormat::json) const {
        const auto* value = format == OutputFormat::backend_json || format == OutputFormat::backend_module ? backend() : &document_;
        if (!value) return fail(ErrorCode::missing, "document has no backend extension");
        std::string json = boost::json::serialize(*value);
        if (format == OutputFormat::backend_module) return "return " + detail::long_string(json) + "\n";
        if (format == OutputFormat::lua_document) return "print(" + detail::long_string(json) + ")\n";
        return json;
    }

    Status write(std::ostream& output, OutputFormat format = OutputFormat::json) const {
        if (format == OutputFormat::json || format == OutputFormat::backend_json) {
            const auto* value = format == OutputFormat::json ? &document_ : backend();
            if (!value) return fail(ErrorCode::missing, "document has no backend extension");
            return detail::write_json(output, *value);
        }
        auto data = serialize(format); if (!data) return Unexpected{data.error()};
        output.write(data->data(), static_cast<std::streamsize>(data->size()));
        if (!output) return fail(ErrorCode::io, "writing Lua document failed");
        return std::monostate{};
    }

    Status save(const Path& path, OutputFormat format = OutputFormat::json) const {
        if ((format == OutputFormat::backend_json || format == OutputFormat::backend_module) && !backend())
            return fail(ErrorCode::missing, "document has no backend extension", path);
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) return fail(ErrorCode::io, "cannot open output", path);
        auto result = write(output, format);
        output.close();
        if (!result) { result.error().path = path; return result; }
        if (!output) return fail(ErrorCode::io, "closing output failed", path);
        return std::monostate{};
    }

    /** Feed any app/transport accepting UTF-8 JSON (HTTP, IPC, sockets, etc.). */
    template<class F> requires std::same_as<std::invoke_result_t<F, std::string_view>, Status>
    Status feed(F&& receiver, OutputFormat format = OutputFormat::json) const {
        auto data = serialize(format); if (!data) return Unexpected{data.error()};
        return std::invoke(std::forward<F>(receiver), std::string_view(*data));
    }

    /** Emit one {arch, id, record} JSON object per line for record-oriented apps. */
    Status write_records(std::ostream& output, Group group) const {
        auto* map = records(group);
        if (!map) return fail(ErrorCode::missing, "backend record group unavailable");
        for (const auto& entry : *map) {
            Json envelope = boost::json::object{{"arch", architecture()}, {"id", entry.key()}, {"record", entry.value()}};
            auto r = detail::write_json(output, envelope); if (!r) return r;
            output.put('\n'); if (!output) return fail(ErrorCode::io, "writing record stream failed");
        }
        return std::monostate{};
    }
};

enum class LoadMode { source, backend };

/** Lazy access to stdspec: discovery does not evaluate/load every architecture. */
class Repository {
    Path directory_;
public:
    explicit Repository(Path directory) : directory_(std::move(directory)) {}
    Result<std::vector<std::string>> architectures(LoadMode mode = LoadMode::source) const {
        const auto dir = mode == LoadMode::backend ? directory_ / "backend" : directory_;
        std::error_code ec;
        std::filesystem::directory_iterator it(dir, ec), end;
        if (ec) return fail(ErrorCode::io, ec.message(), dir);
        std::vector<std::string> result;
        while (it != end) {
            const auto& entry = *it;
            bool regular = entry.is_regular_file(ec);
            if (ec) return fail(ErrorCode::io, ec.message(), entry.path());
            if (regular && entry.path().extension() == (mode == LoadMode::backend ? ".lua" : ".dsymspec"))
                result.push_back(entry.path().stem().string());
            it.increment(ec); if (ec) return fail(ErrorCode::io, ec.message(), dir);
        }
        std::sort(result.begin(), result.end());
        return result;
    }
    Result<Spec> load(std::string_view arch, LoadMode mode = LoadMode::backend, const LoadOptions& options = {}) const {
        if (arch.empty() || std::any_of(arch.begin(), arch.end(), [](char c) {
            return !(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '_';
        })) return fail(ErrorCode::schema, "invalid architecture key");
        auto path = mode == LoadMode::backend ? directory_ / "backend" / (std::string(arch) + ".lua")
                                             : directory_ / (std::string(arch) + ".dsymspec");
        auto result = Spec::load(path, options);
        if (result && result->architecture() != arch) return fail(ErrorCode::schema, "architecture/file mismatch", path);
        return result;
    }
};

/** Duck-typed backend for dsl::Serializer / dsl::Deserializer protocols. */
struct Codec {
    LoadOptions options;
    OutputFormat format = OutputFormat::json;
    Status serialize(const Spec& spec, std::ostream& output) const { return spec.write(output, format); }
    template<class T> requires std::same_as<T, Spec>
    Result<T> deserialize(std::istream& input) const {
        auto data = detail::read(input, options.parse.max_bytes);
        if (!data) return Unexpected{data.error()};
        return Spec::parse(*data, options.format == InputFormat::automatic ? InputFormat::json : options.format, options);
    }
};

} // namespace symspec
