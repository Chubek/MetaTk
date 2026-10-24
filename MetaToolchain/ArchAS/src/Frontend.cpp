#include "Internal.hpp"
#include <algorithm>
#include <cctype>
#include <set>

namespace archas::detail {
void append(std::string &out, std::string_view text, size_t limit) {
    if (out.size() > limit || text.size() > limit - out.size()) throw Error("expansion/output byte limit exceeded");
    out.append(text);
}
std::string directive_name(std::string name) {
    if (!name.empty() && name.front() == '.') name.erase(0, 1);
    if (!mair::ident(name) || name.front() == '.') throw Error("invalid directive name: " + name);
    static const std::set<std::string> reserved = {
        "encode", "form", "macro", "endm", "endmacro", "exitm", "purgem", "rept", "irp", "irpc", "endr",
        "if", "ifdef", "ifndef", "ifc", "ifnc", "ifeqs", "ifnes", "ifb", "ifnb",
        "ifeq", "ifne", "iflt", "ifle", "ifgt", "ifge", "else", "elseif", "endif", "include", "incbin", "set", "equ", "equiv",
        "text", "data", "bss", "section", "pushsection", "popsection", "previous", "global", "globl",
        "local", "weak", "hidden", "extern", "type", "size", "comm", "lcomm", "align", "balign", "p2align",
        "byte", "word", "short", "long", "int", "quad", "2byte", "4byte", "8byte", "ascii", "asciz",
        "string", "zero", "space", "skip", "org", "fill", "file", "loc", "reloc", "intel_syntax", "att_syntax",
        "error", "warning", "abort", "altmacro", "noaltmacro", "lua", "endlua"};
    if (reserved.contains(name)) throw Error("reserved directive: ." + name);
    return "." + name;
}
std::pair<std::string, std::string> head(std::string_view text) {
    auto s = mair::trim(text);
    auto end = s.find_first_of(" \t");
    return {s.substr(0, end), end == s.npos ? std::string{} : mair::trim(std::string_view(s).substr(end))};
}
std::vector<std::string> arguments(std::string_view text, bool whitespace, bool allow_empty) {
    std::vector<std::string> out;
    std::vector<char> stack;
    char quote = 0;
    bool escape = false, comma = false;
    size_t start = 0;
    auto push = [&](size_t end, bool required) {
        auto value = mair::trim(text.substr(start, end - start));
        if (value.empty() && required && !allow_empty) throw Error("empty argument");
        if (!value.empty() || (required && allow_empty)) out.push_back(std::move(value));
    };
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (escape) { escape = false; continue; }
        if (c == '\\') { escape = true; continue; }
        if (quote) { if (c == quote) quote = 0; continue; }
        if (c == '"') { quote = c; continue; }
        if (c == '\'') { i = character(text, i).first - 1; continue; }
        if (c == '(' || c == '[' || c == '{') {
            if (stack.size() >= 64) throw Error("argument nesting limit exceeded");
            stack.push_back(c);
        } else if (c == ')' || c == ']' || c == '}') {
            char expected = c == ')' ? '(' : c == ']' ? '[' : '{';
            if (stack.empty() || stack.back() != expected) throw Error("mismatched argument delimiter");
            stack.pop_back();
        } else if (stack.empty() && (c == ',' || (whitespace && (c == ' ' || c == '\t')))) {
            push(i, c == ',' && (comma || start == 0));
            if (c == ',') comma = true;
            else if (i > start) comma = false;
            start = i + 1;
        }
    }
    if (quote || !stack.empty()) throw Error("unterminated argument");
    push(text.size(), comma);
    return out;
}
Lines raw_lines(std::string_view source, Location start) {
    Lines lines;
    while (!source.empty()) {
        auto end = source.find('\n');
        auto s = source.substr(0, end);
        if (!s.empty() && s.back() == '\r') s.remove_suffix(1);
        lines.push_back({std::string(s), start});
        if (end == source.npos) break;
        source.remove_prefix(end + 1); ++start.line; start.column = 1;
    }
    return lines;
}
Lines lex_lines(const Lines &source, bool arm, size_t limit) {
    Lines out;
    bool block = false;
    Location block_location;
    for (const auto &line : source) {
        std::string text;
        char quote = 0;
        bool escape = false;
        size_t column = line.location.column;
        auto flush = [&]() {
            auto start = text.find_first_not_of(" \t\r");
            if (start != text.npos) {
                if (out.size() >= limit) throw Error("statement limit exceeded", line.location);
                auto loc = line.location; loc.column = column + start;
                out.push_back({mair::trim(text), std::move(loc)});
            }
            text.clear();
        };
        for (size_t i = 0; i < line.text.size(); ++i) {
            char c = line.text[i];
            char next = i + 1 < line.text.size() ? line.text[i + 1] : '\0';
            if (block) {
                if (c == '*' && next == '/') { block = false; ++i; text += ' '; }
                continue;
            }
            if (c == '\0' || (static_cast<unsigned char>(c) < 32 && c != '\t' && c != '\r'))
                throw Error("control character in source", line.location);
            if (escape) { text += c; escape = false; continue; }
            if (c == '\\') { text += c; escape = true; continue; }
            if (quote) { text += c; if (c == quote) quote = 0; continue; }
            if (c == '"') { quote = c; text += c; continue; }
            if (c == '\'') {
                try {
                    size_t end = character(line.text, i).first;
                    text += line.text.substr(i, end - i); i = end - 1; continue;
                } catch (const Error &e) { throw Error(e.what(), line.location); }
            }
            if (c == '/' && next == '*') { block = true; block_location = line.location; ++i; continue; }
            if ((c == '/' && next == '/') || (c == '#' && (!arm || mair::trim(text).empty()))) break;
            if (c == ';') { flush(); column = line.location.column + i + 1; continue; }
            text += c;
        }
        if (quote) throw Error("unterminated quoted string", line.location);
        flush();
    }
    if (block) throw Error("unterminated block comment", block_location);
    return out;
}
Lines lex(std::string_view source, const Location &start, bool arm, size_t limit) {
    size_t newlines = size_t(std::count(source.begin(), source.end(), '\n'));
    if (newlines > limit || (newlines == limit && !source.empty() && !source.ends_with('\n')))
        throw Error("source line limit exceeded", start);
    return lex_lines(raw_lines(source, start), arm, limit);
}

namespace {
struct Parameter { std::string name, value; bool required = false, variadic = false; };
struct Macro { std::vector<Parameter> parameters; Lines body; };
struct Condition { bool parent, active, taken, otherwise = false; };
struct ExitMacro {};
std::string substitute(std::string_view text, const std::map<std::string, std::string> &values,
                       size_t serial, size_t limit) {
    std::string out;
    for (size_t i = 0; i < text.size();) {
        if (text[i] != '\\') { append(out, text.substr(i++, 1), limit); continue; }
        ++i;
        if (i == text.size()) throw Error("incomplete macro substitution");
        if (text[i] == '@') { append(out, std::to_string(serial), limit); ++i; continue; }
        if (text.substr(i).starts_with("()")) { i += 2; continue; }
        size_t start = i;
        while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) ++i;
        auto name = std::string(text.substr(start, i - start));
        auto it = values.find(name);
        if (it != values.end()) append(out, it->second, limit);
        else {
            // String escapes and outer macro parameters survive nested expansion.
            append(out, "\\", limit);
            if (start == i) append(out, text.substr(i++, 1), limit);
            else append(out, name, limit);
        }
    }
    return out;
}
class Frontend {
    const mair::Target &target_;
    const Options &options_;
    Directives directives_;
    Plugins plugins_;
    Lua lua_;
    Constants constants_;
    std::set<std::string> defined_;
    std::map<std::string, Macro> macros_;
    std::set<std::filesystem::path> includes_;
    Translation out_;
    size_t work_ = 0, generated_ = 0, input_ = 0, serial_ = 0;
    bool arm_;

    void tick(const Location &loc) {
        if (work_++ >= options_.limits.statements) throw Error("statement/expansion limit exceeded", loc);
    }
    void emit(Line line) {
        if (generated_ >= options_.limits.bytes || line.text.size() > options_.limits.bytes - generated_ - 1)
            throw Error("expansion/output byte limit exceeded", line.location);
        generated_ += line.text.size() + 1;
        out_.lines.push_back(std::move(line));
    }
    std::filesystem::path resolve(const std::string &name, const Location &loc) const {
        if (name.find('\0') != name.npos) throw Error("NUL in input path", loc);
        std::filesystem::path path(name);
        std::vector<std::filesystem::path> candidates;
        if (path.is_absolute()) candidates.push_back(path);
        else {
            candidates.push_back(std::filesystem::path(loc.file).parent_path() / path);
            for (const auto &root : options_.include_paths) candidates.push_back(root / path);
        }
        for (const auto &p : candidates) if (std::filesystem::is_regular_file(p)) return std::filesystem::canonical(p);
        throw IOError(loc.file + ":" + std::to_string(loc.line) + ": cannot find input: " + name);
    }
    Lines block(const Lines &lines, size_t &i, bool macro) {
        Lines body;
        size_t nesting = 1;
        for (++i; i < lines.size(); ++i) {
            auto [op, rest] = head(lines[i].text);
            (void)rest;
            if ((macro && op == ".macro") || (!macro && (op == ".rept" || op == ".irp" || op == ".irpc"))) ++nesting;
            if ((macro && (op == ".endm" || op == ".endmacro")) || (!macro && op == ".endr")) {
                if (--nesting == 0) return body;
            }
            body.push_back(lines[i]);
        }
        throw Error(macro ? "unterminated .macro" : "unterminated repetition block");
    }
    void macro(const std::string &rest, Lines body) {
        auto end = rest.find_first_of(" \t,");
        auto name = rest.substr(0, end);
        auto parameters = end == rest.npos ? std::string{} : mair::trim(std::string_view(rest).substr(end + 1));
        if (!mair::ident(name) || name.front() == '.' || macros_.contains(name)) throw Error("invalid or duplicate macro: " + name);
        Macro m;
        std::set<std::string> names;
        for (auto p : arguments(parameters, true)) {
            Parameter param;
            auto equal = p.find('=');
            if (equal != p.npos) { param.value = p.substr(equal + 1); p.resize(equal); }
            auto colon = p.find(':');
            if (colon != p.npos) {
                auto qualifier = p.substr(colon + 1); p.resize(colon);
                if (qualifier == "req") param.required = true;
                else if (qualifier == "vararg") param.variadic = true;
                else throw Error("unknown macro parameter qualifier: " + qualifier);
            }
            param.name = p;
            if (!mair::ident(p) || p.find_first_of(".$") != p.npos || !names.insert(p).second)
                throw Error("invalid or duplicate macro parameter: " + p);
            if (!m.parameters.empty() && m.parameters.back().variadic) throw Error("variadic parameter must be last");
            if (param.required && !param.value.empty()) throw Error("required macro parameter cannot have a default");
            m.parameters.push_back(std::move(param));
        }
        m.body = std::move(body); macros_.emplace(std::move(name), std::move(m));
    }
    void invoke(const Macro &definition, const std::string &rest, const Location &loc, size_t depth) {
        // Copy: recursive expansion may purge/redefine the invoked macro.
        const Macro m = definition;
        auto args = arguments(rest, rest.find(',') == rest.npos, true);
        std::map<std::string, std::string> values;
        size_t position = 0;
        bool keyword = false;
        for (size_t i = 0; i < args.size(); ++i) {
            std::string name, value;
            auto eq = args[i].find('=');
            if (eq != args[i].npos && eq > 0 && args[i][eq - 1] != '=' &&
                (eq + 1 == args[i].size() || args[i][eq + 1] != '=') &&
                mair::ident(mair::trim(std::string_view(args[i]).substr(0, eq)))) {
                keyword = true; name = mair::trim(std::string_view(args[i]).substr(0, eq)); value = args[i].substr(eq + 1);
                if (std::none_of(m.parameters.begin(), m.parameters.end(), [&](const auto &p) { return p.name == name; }))
                    throw Error("unknown macro argument: " + name);
            } else {
                if (keyword) throw Error("positional macro argument after keyword argument");
                if (position >= m.parameters.size()) throw Error("too many macro arguments");
                const auto &p = m.parameters[position++];
                name = p.name; value = args[i];
                if (p.variadic) while (++i < args.size()) value += ", " + args[i];
            }
            if (!values.emplace(name, value).second) throw Error("duplicate macro argument: " + name);
        }
        for (const auto &p : m.parameters) if (!values.contains(p.name) || values[p.name].empty()) {
            if (p.required) throw Error("missing required macro argument: " + p.name);
            values[p.name] = p.value;
        }
        Lines expanded;
        size_t serial = serial_++;
        for (const auto &line : m.body) expanded.push_back({substitute(line.text, values, serial, options_.limits.bytes), loc});
        try { process(expanded, depth + 1, true); }
        catch (const ExitMacro &) {}
    }
    void source(std::string_view text, const Location &loc, size_t depth) {
        if (input_ > options_.limits.bytes || text.size() > options_.limits.bytes - input_)
            throw Error("cumulative source byte limit exceeded", loc);
        input_ += text.size();
        size_t newlines = size_t(std::count(text.begin(), text.end(), '\n'));
        if (newlines > options_.limits.statements ||
            (newlines == options_.limits.statements && !text.empty() && !text.ends_with('\n')))
            throw Error("source line limit exceeded", loc);
        process(lex_lines(lua_.expand(text, loc), arm_, options_.limits.statements), depth, false);
    }
    void ordinary(Line line, std::string op, std::string rest) {
        // MetaArchIR's undotted directives retain fixed-width semantics.
        static const std::map<std::string, std::string> portable = {
            {"section", ".section"}, {"global", ".globl"}, {"globl", ".globl"}, {"align", ".balign"},
            {"balign", ".balign"}, {"ascii", ".ascii"}, {"byte", ".byte"}, {"word", ".2byte"},
            {"long", ".4byte"}, {"quad", ".8byte"}, {"encode", ".encode"}, {"form", ".form"}};
        if (auto it = portable.find(op); it != portable.end()) op = it->second;
        if (op == ".encode" || op == ".form") {
            auto parts = arguments(rest);
            if (parts.empty()) throw Error("expected encoder name or form index");
            rest = parts[0];
            for (size_t j = 1; j < parts.size(); ++j) {
                auto eq = parts[j].find('=');
                if (eq == parts[j].npos) throw Error("expected parameter=value");
                auto key = mair::trim(std::string_view(parts[j]).substr(0, eq));
                auto value = mair::trim(std::string_view(parts[j]).substr(eq + 1));
                if (!value.empty() && (value.front() == '%' || value.front() == '$' || value.front() == '#')) value.erase(0, 1);
                if (!value.starts_with('@') && !target_.spec().find_register(value)) value = std::to_string(evaluate(value, constants_));
                rest += ", " + key + "=" + value;
            }
        }
        line.text = op + (rest.empty() ? "" : " " + rest);
        emit(std::move(line));
    }
    void process(const Lines &lines, size_t depth, bool in_macro) {
        if (depth > options_.limits.expansion_depth) throw Error("macro/include/directive expansion depth exceeded");
        std::vector<Condition> conditions;
        auto active = [&]() { return conditions.empty() || conditions.back().active; };
        for (size_t i = 0; i < lines.size(); ++i) {
            const auto loc = lines[i].location;
            try {
                tick(loc);
                auto [op, rest] = head(lines[i].text);
                static const std::set<std::string> ifs = {".if", ".ifdef", ".ifndef", ".ifc", ".ifnc",
                    ".ifeqs", ".ifnes", ".ifb", ".ifnb", ".ifeq", ".ifne", ".iflt", ".ifle", ".ifgt", ".ifge"};
                if (ifs.contains(op)) {
                    bool parent = active(), condition = false;
                    if (parent) {
                        if (op == ".ifdef" || op == ".ifndef") {
                            if (!mair::ident(rest)) throw Error("expected symbol name for " + op);
                            condition = defined_.contains(rest) || macros_.contains(rest);
                            if (op == ".ifndef") condition = !condition;
                        } else if (op == ".ifb" || op == ".ifnb") {
                            condition = rest.empty();
                            if (op == ".ifnb") condition = !condition;
                        } else if (op == ".ifc" || op == ".ifnc" || op == ".ifeqs" || op == ".ifnes") {
                            auto values = arguments(rest, false, true);
                            if (values.size() != 2) throw Error("expected two strings for " + op);
                            if (op == ".ifeqs" || op == ".ifnes")
                                for (auto &value : values) value = mair::unquote(value);
                            condition = values[0] == values[1];
                            if (op == ".ifnc" || op == ".ifnes") condition = !condition;
                        } else {
                            auto n = evaluate(rest, constants_);
                            condition = op == ".ifeq" ? n == 0 : op == ".iflt" ? n < 0 : op == ".ifle" ? n <= 0 :
                                        op == ".ifgt" ? n > 0 : op == ".ifge" ? n >= 0 : n != 0;
                        }
                    }
                    conditions.push_back({parent, parent && condition, condition, false}); continue;
                }
                if (op == ".else" || op == ".elseif") {
                    if (conditions.empty() || conditions.back().otherwise) throw Error("unexpected " + op);
                    auto &c = conditions.back();
                    if (op == ".else" && !rest.empty()) throw Error(".else takes no arguments");
                    bool take = c.parent && !c.taken;
                    if (op == ".elseif" && take) take = evaluate(rest, constants_) != 0;
                    c.active = take; c.taken = c.taken || take; c.otherwise = op == ".else"; continue;
                }
                if (op == ".endif") {
                    if (conditions.empty() || !rest.empty()) throw Error("unexpected .endif");
                    conditions.pop_back(); continue;
                }
                if (op == ".macro") {
                    auto body = block(lines, i, true);
                    if (active()) macro(rest, std::move(body));
                    continue;
                }
                if (op == ".rept" || op == ".irp" || op == ".irpc") {
                    auto body = block(lines, i, false);
                    if (!active()) continue;
                    size_t count = 0;
                    std::string parameter;
                    std::vector<std::string> values;
                    if (op == ".rept") {
                        auto n = evaluate(rest, constants_);
                        if (n < 0 || uint64_t(n) > options_.limits.statements) throw Error("repetition count out of range");
                        count = size_t(n);
                    } else {
                        auto args = arguments(rest);
                        if (args.size() < 2 || !mair::ident(args.front())) throw Error("expected repetition parameter and values");
                        parameter = args.front(); args.erase(args.begin());
                        if (op == ".irp") values = std::move(args);
                        else {
                            if (args.size() != 1) throw Error(".irpc takes one string");
                            auto chars = args[0].starts_with('"') ? mair::unquote(args[0]) : args[0];
                            for (char c : chars) values.emplace_back(1, c);
                        }
                        count = values.size();
                    }
                    for (size_t n = 0; n < count; ++n) {
                        tick(loc);
                        if (parameter.empty()) process(body, depth + 1, in_macro);
                        else {
                            Lines expanded;
                            for (const auto &l : body) expanded.push_back({substitute(l.text, {{parameter, values[n]}}, serial_, options_.limits.bytes), l.location});
                            process(expanded, depth + 1, in_macro);
                        }
                    }
                    continue;
                }
                if (!active()) continue;
                if (op == ".endm" || op == ".endmacro" || op == ".endr") throw Error("unexpected " + op);
                if (op == ".exitm") {
                    if (!in_macro || !rest.empty()) throw Error(".exitm requires a macro invocation and no arguments");
                    throw ExitMacro{};
                }
                if (op == ".purgem") { if (!macros_.erase(rest)) throw Error("unknown macro: " + rest); continue; }
                if (op == ".altmacro" || op == ".noaltmacro") throw Error("alternate GAS macro mode is not supported; use .macro or Lua templates");
                // A label may precede another statement, including a macro call.
                auto colon = lines[i].text.find(':');
                if (colon != std::string::npos) {
                    auto name = mair::trim(std::string_view(lines[i].text).substr(0, colon));
                    bool numeric = !name.empty() && std::all_of(name.begin(), name.end(), [](char c) { return c >= '0' && c <= '9'; });
                    if (mair::ident(name) || numeric) {
                        defined_.insert(name); emit({name + ":", loc});
                        auto tail = mair::trim(std::string_view(lines[i].text).substr(colon + 1));
                        if (!tail.empty()) process({{tail, loc}}, depth + 1, in_macro);
                        continue;
                    }
                }
                if (auto it = macros_.find(op); it != macros_.end()) { invoke(it->second, rest, loc, depth); continue; }
                if (op == ".include") {
                    auto path = resolve(mair::unquote(rest), loc);
                    if (!includes_.insert(path).second) throw Error("recursive include: " + path.string());
                    out_.dependencies.push_back(path);
                    auto data = read_file(path, options_.limits.bytes);
                    source(data, {path.string(), 1, 1}, depth + 1);
                    includes_.erase(path); continue;
                }
                if (op == ".incbin") {
                    auto args = arguments(rest);
                    if (args.empty()) throw Error(".incbin requires a filename");
                    auto path = resolve(mair::unquote(args[0]), loc);
                    out_.dependencies.push_back(path);
                    rest = mair::quote(path.string());
                    for (size_t j = 1; j < args.size(); ++j) rest += ", " + args[j];
                }
                if (op == ".set" || op == ".equ" || op == ".equiv") {
                    auto args = arguments(rest);
                    if (args.size() != 2 || !mair::ident(args[0])) throw Error("expected constant name, expression");
                    if (op == ".equiv" && defined_.contains(args[0])) throw Error("constant already defined: " + args[0]);
                    try {
                        constants_[args[0]] = evaluate(args[1], constants_);
                        rest = args[0] + ", " + std::to_string(constants_[args[0]]);
                    } catch (const NonAbsolute &) {
                        // GAS can resolve section-relative .set/.equ aliases
                        // after layout; they are not preprocessor constants.
                        constants_.erase(args[0]);
                    }
                    defined_.insert(args[0]);
                }
                if (op == ".error" || op == ".abort") throw Error(rest.empty() ? "assembly aborted" : rest);
                if (auto it = directives_.find(op); it != directives_.end()) {
                    auto expansion = it->second(rest, {target_, loc, options_.syntax});
                    if (expansion.size() > options_.limits.bytes) throw Error("directive expansion byte limit exceeded");
                    process(lex(expansion, loc, arm_, options_.limits.statements), depth + 1, in_macro); continue;
                }
                ordinary(lines[i], op, rest);
            } catch (const Error &e) {
                if (!e.location.file.empty()) throw;
                throw Error(e.what(), loc);
            }
        }
        if (!conditions.empty()) throw Error("unterminated .if", lines.empty() ? Location{} : lines.back().location);
    }
public:
    Frontend(const mair::Target &target, const Options &options, Directives directives)
        : target_(target), options_(options), directives_(std::move(directives)),
          plugins_(target, options, directives_), lua_(target, options, directives_), constants_(options.defines),
          arm_(std::string_view(target.description().arch).starts_with("arm") || std::string_view(target.description().arch) == "aarch64") {
        for (const auto &[name, value] : constants_) {
            if (!mair::ident(name)) throw Error("invalid constant name: " + name);
            defined_.insert(name);
            emit({".set " + name + ", " + std::to_string(value), {"<command-line>", 1, 1}});
        }
    }
    Translation run(std::string_view text, const std::string &file) {
        for (const auto &path : options_.plugins) out_.dependencies.push_back(std::filesystem::absolute(path));
        for (const auto &path : options_.lua_files) {
            out_.dependencies.push_back(std::filesystem::absolute(path));
            auto script = read_file(path, options_.limits.bytes);
            if (input_ > options_.limits.bytes || script.size() > options_.limits.bytes - input_) throw Error("cumulative source byte limit exceeded");
            input_ += script.size();
            auto emitted = lua_.script(script, path.string());
            process(lex(emitted, {path.string(), 1, 1}, arm_, options_.limits.statements), 0, false);
        }
        if (std::filesystem::is_regular_file(file)) {
            auto path = std::filesystem::canonical(file);
            includes_.insert(path); out_.dependencies.push_back(path);
        }
        source(text, {file, 1, 1}, 0);
        return std::move(out_);
    }
};
} // namespace
Translation preprocess(std::string_view source, const std::string &file, const mair::Target &target,
                       const Options &options, Directives directives) {
    return Frontend(target, options, std::move(directives)).run(source, file);
}
} // namespace archas::detail
