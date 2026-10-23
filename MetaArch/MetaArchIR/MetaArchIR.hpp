#pragma once
// Portable assembly, C++20. Link MetaArch::ir. No instruction selection or passes.
#include "../MetaArch.hpp"
#include <DSLtk.hpp>
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace mair {
struct Location { std::string file; size_t line = 0, column = 0; };
struct Error : std::runtime_error {
    Location location;
    explicit Error(std::string message, Location where = {})
        : std::runtime_error(std::move(message)), location(std::move(where)) {}
};
struct Limits {
    size_t source_bytes = 16 * 1024 * 1024, statements = 1000000;
    size_t nesting = 64, output_bytes = 16 * 1024 * 1024;
};
inline bool ident(std::string_view s) {
    auto alpha = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '.'; };
    if (s.empty() || !alpha(s.front())) return false;
    for (char c : s) if (!alpha(c) && !(c >= '0' && c <= '9') && c != '$') return false;
    return true;
}
inline std::string trim(std::string_view s) {
    auto a = s.find_first_not_of(" \t\r\n");
    return a == s.npos ? std::string{} : std::string(s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1));
}
namespace detail {
inline void name_check(std::string_view s) { if (!ident(s)) throw Error("invalid name: " + std::string(s)); }
inline void single_line(std::string_view s) {
    for (unsigned char c : s) if (c < 32 && c != '\t') throw Error("control character in assembly text");
    if (s.find(';') != s.npos || s.find("//") != s.npos) throw Error("statement separator in operand");
}
inline int64_t integer(std::string_view s) {
    bool neg = false;
    if (!s.empty() && (s.front() == '-' || s.front() == '+')) { neg = s.front() == '-'; s.remove_prefix(1); }
    int base = 10;
    if (s.starts_with("0x")) { base = 16; s.remove_prefix(2); }
    else if (s.starts_with("0b")) { base = 2; s.remove_prefix(2); }
    else if (s.starts_with("0o")) { base = 8; s.remove_prefix(2); }
    uint64_t n = 0, bound = uint64_t(INT64_MAX) + (neg ? 1 : 0);
    auto r = std::from_chars(s.data(), s.data() + s.size(), n, base);
    if (s.empty() || r.ec != std::errc{} || r.ptr != s.data() + s.size() || n > bound) throw Error("expected signed 64-bit integer");
    if (neg && n == bound) return INT64_MIN;
    return neg ? -int64_t(n) : int64_t(n);
}
template<class T> inline T checked(metaarch::Result<T> r) {
    if (r) return std::move(*r.get());
    std::string message;
    Location loc;
    for (const auto &d : r.error()->diagnostics) {
        if (!message.empty()) message += "; ";
        message += d.message;
        if (loc.file.empty()) loc = {d.file, d.line, d.column};
    }
    throw Error(message.empty() ? "MetaArch operation failed" : message, loc);
}
inline void spaces(dsl::ParsecInput &in) {
    (*dsl::satisfy([](char c) { return c == ' ' || c == '\t'; }, "space"))(in);
}
inline const auto identifier = dsl::parser([](dsl::ParsecInput &in) -> dsl::ExpectedResult<std::string> {
    size_t begin = in.pos;
    auto first = dsl::satisfy([](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.'; }, "identifier");
    if (!first(in)) return dsl::fail_expected<std::string>(in, "identifier");
    (*dsl::satisfy([](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '$'; }, "identifier"))(in);
    return std::string(in.get_span(begin));
});
} // namespace detail

// Integer or physical register name. There are no virtual registers.
struct SymbolRef { std::string name; int64_t addend = 0; };
inline SymbolRef symbol(std::string name, int64_t addend = 0) {
    detail::name_check(name); return {std::move(name), addend};
}
using Operand = std::variant<int64_t, std::string, SymbolRef>;
struct Binding { std::string name; Operand value; };
inline Operand reg(std::string name) { detail::name_check(name); return name; }
inline Operand imm(int64_t value) { return value; }
struct Statement {
    enum class Kind { instruction, encoder, form, label, global, section, align,
                      ascii, byte, word, long_, quad, comment } kind;
    std::string name;
    std::vector<std::string> args;
    Location location;
};
inline std::vector<std::string> split_operands(std::string_view text, size_t max_depth = 64) {
    std::vector<std::string> out;
    std::vector<char> stack;
    bool quoted = false, escaped = false;
    size_t begin = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (escaped) { escaped = false; continue; }
        if (quoted && c == '\\') { escaped = true; continue; }
        if (c == '"') { quoted = !quoted; continue; }
        if (quoted) continue;
        if (c == '(' || c == '[' || c == '{') {
            if (stack.size() >= max_depth) throw Error("operand nesting limit exceeded");
            stack.push_back(c);
        } else if (c == ')' || c == ']' || c == '}') {
            char open = c == ')' ? '(' : c == ']' ? '[' : '{';
            if (stack.empty() || stack.back() != open) throw Error("mismatched operand delimiter");
            stack.pop_back();
        } else if (c == ',' && stack.empty()) {
            auto v = trim(text.substr(begin, i - begin));
            if (v.empty()) throw Error("empty operand");
            out.push_back(std::move(v)); begin = i + 1;
        }
    }
    if (quoted || escaped || !stack.empty()) throw Error("unterminated operand");
    auto tail = trim(text.substr(begin));
    if (tail.empty() && !out.empty()) throw Error("trailing operand separator");
    if (!tail.empty()) out.push_back(std::move(tail));
    return out;
}
inline std::string quote(std::string_view s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c >= 32 && c < 127) out += char(c);
        else { out += '\\'; out += char('0' + ((c >> 6) & 7)); out += char('0' + ((c >> 3) & 7)); out += char('0' + (c & 7)); }
    }
    return out + '"';
}
inline std::string unquote(std::string_view s) {
    if (s.size() < 2 || s.front() != '"' || s.back() != '"') throw Error("expected quoted string");
    std::string out;
    for (size_t i = 1; i + 1 < s.size(); ++i) {
        unsigned char c = s[i];
        if (c == '"' || c < 32) throw Error("invalid string character");
        if (c != '\\') { out += char(c); continue; }
        if (++i >= s.size() - 1) throw Error("incomplete escape");
        c = s[i];
        switch (c) {
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case '"': case '\\': out += char(c); break;
        default: {
            if (c < '0' || c > '7') throw Error("unknown string escape");
            unsigned value = c - '0';
            for (unsigned n = 1; n < 3 && i + 1 < s.size() - 1 && s[i + 1] >= '0' && s[i + 1] <= '7'; ++n)
                value = value * 8 + unsigned(s[++i] - '0');
            if (value > 255) throw Error("octal escape exceeds one byte");
            out += char(value); break;
        }
        }
    }
    return out;
}

// Owns all text. Mutators validate before append; const reads are thread safe.
class Program {
    std::vector<Statement> code_;
    std::set<std::string> labels_;
    Program &append(Statement::Kind kind, std::string name, std::vector<std::string> args = {}) {
        code_.push_back({kind, std::move(name), std::move(args), {}}); return *this;
    }
public:
    const std::vector<Statement> &statements() const noexcept { return code_; }
    bool empty() const noexcept { return code_.empty(); }
    size_t size() const noexcept { return code_.size(); }
    void clear() noexcept { code_.clear(); labels_.clear(); }
    Program &at(Location loc) { if (!code_.empty()) code_.back().location = std::move(loc); return *this; }
    Program &emit(std::string op, std::vector<std::string> args = {}) {
        detail::name_check(op);
        for (auto &a : args) {
            if (trim(a).empty()) throw Error("empty operand");
            detail::single_line(a);
            if (split_operands(a).size() != 1) throw Error("one operand expected");
        }
        return append(Statement::Kind::instruction, std::move(op), std::move(args));
    }
    template<class... A> Program &ins(std::string op, A &&...args) {
        return emit(std::move(op), {std::string(std::forward<A>(args))...});
    }
    // Low-level VM constraints apply; inputs are explicitly named.
    Program &encode(std::string encoder, const std::vector<Binding> &bindings = {}) {
        detail::name_check(encoder);
        std::vector<std::string> args;
        std::set<std::string> names;
        for (const auto &b : bindings) {
            detail::name_check(b.name);
            if (!names.insert(b.name).second) throw Error("duplicate parameter: " + b.name);
            std::string value;
            if (auto n = std::get_if<int64_t>(&b.value)) value = std::to_string(*n);
            else if (auto reg = std::get_if<std::string>(&b.value)) { value = *reg; detail::name_check(value); }
            else {
                const auto &ref = std::get<SymbolRef>(b.value);
                detail::name_check(ref.name);
                value = "@" + ref.name;
                if (ref.addend) value += (ref.addend > 0 ? "+" : "") + std::to_string(ref.addend);
            }
            args.push_back(b.name + "=" + value);
        }
        return append(Statement::Kind::encoder, std::move(encoder), std::move(args));
    }
    // Form operands in declared order, each explicitly mapped to an encoder input.
    Program &form(size_t index, const std::vector<Binding> &bindings = {}) {
        if (index > uint64_t(INT64_MAX)) throw Error("form index out of range");
        auto name = std::to_string(index);
        encode("form", bindings);
        code_.back().kind = Statement::Kind::form;
        code_.back().name = std::move(name); return *this;
    }
    Program &label(std::string s) {
        detail::name_check(s);
        auto [it, inserted] = labels_.insert(s);
        if (!inserted) throw Error("duplicate label: " + s);
        try { return append(Statement::Kind::label, std::move(s)); }
        catch (...) { labels_.erase(it); throw; }
    }
    Program &global(std::string s) { detail::name_check(s); return append(Statement::Kind::global, std::move(s)); }
    Program &section(std::string s) {
        detail::name_check(s); if (s.front() != '.') throw Error("section name must start with '.'");
        return append(Statement::Kind::section, std::move(s));
    }
    Program &align(unsigned n) {
        if (!n || n > 65536 || (n & (n - 1))) throw Error("alignment must be a power of two <= 65536");
        return append(Statement::Kind::align, std::to_string(n));
    }
    Program &data(std::string directive, std::vector<std::string> values) {
        auto k = directive == "byte" ? Statement::Kind::byte : directive == "word" ? Statement::Kind::word :
                 directive == "long" ? Statement::Kind::long_ : directive == "quad" ? Statement::Kind::quad : Statement::Kind::comment;
        if (k == Statement::Kind::comment) throw Error("unknown data directive: " + directive);
        if (values.empty()) throw Error("empty data");
        size_t width = k == Statement::Kind::byte ? 1 : k == Statement::Kind::word ? 2 : k == Statement::Kind::long_ ? 4 : 8;
        for (auto &v : values) if (!ident(v)) {
            auto n = detail::integer(v);
            if (width < 8 && (n < -(int64_t(1) << (width * 8 - 1)) ||
                (n >= 0 && uint64_t(n) >= (uint64_t(1) << (width * 8))))) throw Error("data value does not fit directive");
            v = std::to_string(n);
        }
        return append(k, {}, std::move(values));
    }
    Program &ascii(std::string s) { return append(Statement::Kind::ascii, std::move(s)); }
    Program &comment(std::string s) { detail::single_line(s); return append(Statement::Kind::comment, std::move(s)); }
};
namespace detail {
inline std::vector<Binding> bindings(const std::vector<std::string> &args) {
    std::vector<Binding> result;
    for (const auto &a : args) {
        auto eq = a.find('=');
        if (eq == a.npos) throw Error("expected parameter=value");
        auto name = trim(std::string_view(a).substr(0, eq)), value = trim(std::string_view(a).substr(eq + 1));
        name_check(name);
        Operand operand;
        if (value.starts_with('@')) {
            auto end = value.find_first_of("+-", 1);
            auto name = value.substr(1, end == value.npos ? end : end - 1);
            operand = symbol(name, end == value.npos ? 0 : integer(std::string_view(value).substr(end)));
        } else if (ident(value)) operand = value; else operand = integer(value);
        result.push_back({std::move(name), std::move(operand)});
    }
    return result;
}
} // namespace detail

// Line-oriented DSLtk parser; no borrowed source survives this call.
inline Program parse(std::string_view source, const std::string &file = "<memory>", Limits limits = {}) {
    if (source.size() > limits.source_bytes) throw Error("source byte limit exceeded", {file, 1, 1});
    Program program;
    size_t line = 0;
    while (!source.empty()) {
        ++line;
        auto end = source.find('\n');
        auto text = source.substr(0, end);
        if (!text.empty() && text.back() == '\r') text.remove_suffix(1);
        size_t column = 1;
        try {
            const auto statement = dsl::parser([&](dsl::ParsecInput &in) -> dsl::ExpectedResult<bool> {
                detail::spaces(in);
                if (in.eof() || in.peek() == '#' || in.source.substr(in.pos).starts_with("//")) { in.pos = in.source.size(); return true; }
                column = in.pos + 1;
                auto key = detail::identifier(in);
                if (!key) return dsl::fail_expected<bool>(in, "instruction, directive or label");
                detail::spaces(in);
                if (dsl::ch(':')(in)) {
                    program.label(*key).at({file, line, column});
                    detail::spaces(in);
                    if (in.eof() || in.source.substr(in.pos).starts_with("//") || in.peek() == '#') { in.pos = in.source.size(); return true; }
                    return dsl::fail_expected<bool>(in, "end of label line");
                }
                size_t start = in.pos;
                bool quoted = false, escaped = false;
                const auto character = dsl::satisfy([](char) { return true; }, "operand character");
                while (!in.eof()) {
                    if (!quoted && in.source.substr(in.pos).starts_with("//")) break;
                    char c = *character(in);
                    if (escaped) { escaped = false; continue; }
                    if (quoted && c == '\\') escaped = true;
                    else if (c == '"') quoted = !quoted;
                }
                if (quoted) return dsl::fail_expected<bool>(in, "closing quote");
                auto rest = trim(in.source.substr(start, in.pos - start));
                in.pos = in.source.size();
                auto k = *key;
                if (k.front() == '.') k.erase(0, 1);
                if (k == "2byte") k = "word";
                else if (k == "4byte") k = "long";
                else if (k == "8byte") k = "quad";
                if (k == "section") program.section(rest);
                else if (k == "global" || k == "globl") program.global(rest);
                else if (k == "align" || k == "balign") {
                    auto n = detail::integer(rest);
                    if (n < 1 || n > 65536) throw Error("alignment out of range");
                    program.align(unsigned(n));
                } else if (k == "ascii") program.ascii(unquote(rest));
                else if (k == "byte" || k == "word" || k == "long" || k == "quad") program.data(k, split_operands(rest, limits.nesting));
                else if (k == "encode" || k == "form") {
                    auto args = split_operands(rest, limits.nesting);
                    if (args.empty()) throw Error("expected encoder name or form index");
                    auto name = args.front(); args.erase(args.begin());
                    auto values = detail::bindings(args);
                    if (k == "encode") program.encode(name, values);
                    else {
                        auto n = detail::integer(name);
                        if (n < 0 || uint64_t(n) > SIZE_MAX) throw Error("form index out of range");
                        program.form(size_t(n), values);
                    }
                } else program.emit(*key, split_operands(rest, limits.nesting));
                program.at({file, line, column}); return true;
            });
            auto result = dsl::run_parser(statement, text);
            if (!result.value) {
                std::string message = "expected ";
                for (const auto &e : result.error.expected) message += e + " ";
                throw Error(message, {file, line, result.error.pos + 1});
            }
            if (program.size() > limits.statements) throw Error("statement limit exceeded");
        } catch (const Error &e) {
            if (e.location.line) throw;
            throw Error(e.what(), {file, line, column});
        }
        if (end == source.npos) break;
        source.remove_prefix(end + 1);
    }
    return program;
}

// Copies share an immutable specification; no lazy backend loading.
class Target {
    std::shared_ptr<const metaarch::Spec> spec_;
    std::string name_;
    size_t index_ = 0;
public:
    Target() = default;
    Target(metaarch::Spec spec, std::string name, size_t index = 0)
        : spec_(std::make_shared<metaarch::Spec>(std::move(spec))), name_(std::move(name)), index_(index) {
        if (!spec_->get() || index >= spec_->get()->target_count) throw Error("target index out of range");
        detail::name_check(name_);
    }
    const metaarch::Spec &spec() const { if (!spec_) throw Error("target database not loaded"); return *spec_; }
    const DsymTarget &description() const { return spec().get()->targets[index_]; }
    const std::string &name() const noexcept { return name_; }
    size_t index() const noexcept { return index_; }
    std::vector<size_t> forms(std::string_view mnemonic) const {
        std::vector<size_t> result;
        const auto *db = spec().get();
        for (size_t i = 0; i < db->form_count; ++i)
            if (mnemonic == db->forms[i].mnemonic) result.push_back(i);
        return result;
    }
    std::vector<std::string> parameters(const std::string &encoder) const {
        detail::name_check(encoder);
        const auto *db = spec().get();
        auto index = dsym_spec_find_encoder(db, encoder.c_str());
        if (index < 0) throw Error("unknown encoder: " + encoder);
        auto *program = db->encoders[index].program;
        if (!program) throw Error("encoder has no compiled program: " + encoder);
        std::vector<std::string> result;
        for (size_t i = 0; i < program->param_count; ++i) result.emplace_back(program->params[i]);
        return result;
    }
    DsymRegister register_info(const std::string &name) const {
        detail::name_check(name);
        auto *record = spec().find_register(name);
        if (!record) throw Error("unknown register: " + name);
        return *record; // String fields borrow from this Target's shared spec.
    }
    bool supports(std::string_view mnemonic) const {
        const auto *db = spec().get();
        for (size_t i = 0; i < db->mnemonic_count; ++i) {
            const auto &m = db->mnemonics[i];
            auto equal = [&](std::string_view b) {
                if (b.size() != mnemonic.size()) return false;
                for (size_t j = 0; j < b.size(); ++j) {
                    auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? char(c + ('a' - 'A')) : c; };
                    if ((m.ci ? lower(b[j]) : b[j]) != (m.ci ? lower(mnemonic[j]) : mnemonic[j])) return false;
                }
                return true;
            };
            if (equal(m.name)) return true;
            for (size_t j = 0; j < m.aliases.count; ++j) if (equal(m.aliases.items[j])) return true;
        }
        if (db->asmtk && mnemonic.starts_with('j'))
            for (size_t i = 0; i < db->asmtk->condition_count; ++i)
                for (size_t j = 0; j < db->asmtk->conditions[i].names.count; ++j)
                    if (mnemonic == db->asmtk->conditions[i].names.items[j]) return true;
        // GAS integer-size suffixes are an explicit native x86 syntax convention.
        if ((std::string_view(description().arch) == "amd64" || std::string_view(description().arch) == "x86") &&
            mnemonic.size() > 1 && std::string_view("bwlq").find(mnemonic.back()) != std::string_view::npos) {
            auto base = mnemonic.substr(0, mnemonic.size() - 1);
            for (size_t i = 0; i < db->mnemonic_count; ++i) if (base == db->mnemonics[i].name) return true;
        }
        return false;
    }
};
inline std::filesystem::path database_path(std::filesystem::path root = {}) {
    if (!root.empty()) return root;
    if (const char *env = std::getenv("METAARCHDB")) return env;
    if (std::filesystem::is_directory("MetaArchDB")) return "MetaArchDB";
    if (std::filesystem::is_directory("../MetaArchDB")) return "../MetaArchDB";
    throw Error("MetaArchDB not found; specify --db or METAARCHDB");
}
inline Target target(std::string name, std::filesystem::path root = {}, size_t index = 0) {
    if (!ident(name) || name == "." || name == "..") throw Error("invalid target name");
    auto file = database_path(std::move(root)) / name / (name + ".march");
    return Target(detail::checked(metaarch::Spec::load_dsl(file.string())), name, index);
}
inline std::vector<std::string> targets(std::filesystem::path root = {}) {
    std::vector<std::string> out;
    for (const auto &entry : std::filesystem::directory_iterator(database_path(std::move(root)))) {
        auto name = entry.path().filename().string();
        if (entry.is_directory() && ident(name) && std::filesystem::is_regular_file(entry.path() / (name + ".march"))) out.push_back(name);
    }
    std::sort(out.begin(), out.end()); return out;
}
namespace detail {
struct Encoded : metaarch::Encoded { std::map<std::string, std::string> symbols; };
inline Encoded encode(const Statement &s, const Target &t) {
    auto b = bindings(s.args);
    Encoded result;
    std::vector<std::string> names;
    std::vector<metaarch::Operand> operands;
    std::vector<metaarch::Binding> inputs;
    for (const auto &v : b) {
        metaarch::Operand operand;
        if (auto n = std::get_if<int64_t>(&v.value)) operand = *n;
        else if (auto reg = std::get_if<std::string>(&v.value)) operand = *reg;
        else {
            if (s.kind == Statement::Kind::form) throw Error("symbolic inputs require the low-level encode directive");
            const auto &ref = std::get<SymbolRef>(v.value);
            result.symbols.emplace(v.name, ref.name); operand = ref.addend;
        }
        names.push_back(v.name); operands.push_back(operand); inputs.push_back({v.name, std::move(operand)});
    }
    static_cast<metaarch::Encoded &>(result) = s.kind == Statement::Kind::encoder ?
        checked(t.spec().encode(s.name, inputs, t.index())) :
        checked(t.spec().encode_form(size_t(integer(s.name)), names, operands, t.index()));
    for (const auto &[parameter, name] : result.symbols) {
        (void)name;
        if (std::none_of(result.relocations.begin(), result.relocations.end(), [&](const auto &r) { return r.parameter == parameter; }))
            throw Error("symbol input is not an emitted relocation: " + parameter);
    }
    return result;
}
inline const char *directive(Statement::Kind k) {
    return k == Statement::Kind::byte ? "byte" : k == Statement::Kind::word ? "2byte" : k == Statement::Kind::long_ ? "4byte" : "8byte";
}
} // namespace detail

// GAS-style text. Native operands retain their assembler dialect. Checked forms
// emit exact bytes; relocation-bearing encodings must use assemble().
inline std::string render(const Program &p, const Target &t, Limits limits = {}) {
    (void)t.spec();
    if (p.size() > limits.statements) throw Error("statement limit exceeded");
    std::string out = "# MetaArchIR target: " + t.name() + "\n";
    for (const auto &s : p.statements()) {
        try {
            std::ostringstream o;
            o.imbue(std::locale::classic());
            switch (s.kind) {
            case Statement::Kind::instruction:
                if (!t.supports(s.name)) throw Error("instruction '" + s.name + "' is not declared for " + t.name());
                o << "    " << s.name;
                for (size_t i = 0; i < s.args.size(); ++i) o << (i ? ", " : " ") << s.args[i];
                o << '\n'; break;
            case Statement::Kind::encoder: case Statement::Kind::form: {
                auto v = detail::encode(s, t);
                if (!v.relocations.empty()) throw Error("relocatable encoding requires assemble() image output");
                if (!v.bytes.empty()) {
                    o << ".byte ";
                    for (size_t i = 0; i < v.bytes.size(); ++i) o << (i ? ", " : "") << unsigned(v.bytes[i]);
                    o << '\n';
                }
                break;
            }
            case Statement::Kind::label: o << s.name << ":\n"; break;
            case Statement::Kind::global: o << ".globl " << s.name << '\n'; break;
            case Statement::Kind::section: o << ".section " << s.name << '\n'; break;
            case Statement::Kind::align: o << ".balign " << s.name << '\n'; break;
            case Statement::Kind::ascii: o << ".ascii " << quote(s.name) << '\n'; break;
            case Statement::Kind::comment: o << "# " << s.name << '\n'; break;
            default:
                o << '.' << detail::directive(s.kind) << ' ';
                for (size_t i = 0; i < s.args.size(); ++i) o << (i ? ", " : "") << s.args[i];
                o << '\n'; break;
            }
            auto text = o.str();
            if (out.size() > limits.output_bytes || text.size() > limits.output_bytes - out.size()) throw Error("output byte limit exceeded");
            out += text;
        } catch (const Error &e) { throw Error(e.what(), s.location); }
    }
    if (out.size() > limits.output_bytes) throw Error("output byte limit exceeded");
    return out;
}

struct Section { std::string name; size_t alignment = 1; std::vector<uint8_t> bytes; };
struct Symbol { std::string name; size_t section, offset; bool global = false; };
struct Relocation {
    size_t section, offset, size;
    std::string name, parameter, symbol;
    int64_t value;
    size_t bit_offset, bit_width;
    DsymEndian endian;
    bool pc_relative;
};
struct Image {
    std::vector<Section> sections;
    std::vector<Symbol> symbols;
    std::vector<Relocation> relocations;
    // No implicit linker: reject information loss in flat output.
    std::vector<uint8_t> flat() const {
        if (!relocations.empty()) throw Error("flat output cannot contain unresolved relocations");
        if (sections.size() > 1) throw Error("flat output requires one section");
        return sections.empty() ? std::vector<uint8_t>{} : sections.front().bytes;
    }
};
// Section-relative layout. Database relocation expressions are left to a linker.
inline Image assemble(const Program &p, const Target &t, Limits limits = {}) {
    if (p.size() > limits.statements) throw Error("statement limit exceeded");
    const bool little = t.description().byte_order == DSYM_BO_LITTLE || t.description().byte_order == DSYM_BO_LE;
    Image out;
    size_t current = 0, total = 0;
    std::set<std::string> globals;
    auto select = [&](const std::string &name) {
        for (size_t i = 0; i < out.sections.size(); ++i) if (out.sections[i].name == name) { current = i; return; }
        current = out.sections.size(); out.sections.push_back({name, 1, {}});
    };
    auto room = [&](size_t n) {
        if (total > limits.output_bytes || n > limits.output_bytes - total) throw Error("output byte limit exceeded");
        total += n;
    };
    for (const auto &s : p.statements()) {
        try {
            if (s.kind == Statement::Kind::section) { select(s.name); continue; }
            if (s.kind == Statement::Kind::global) { globals.insert(s.name); continue; }
            if (s.kind == Statement::Kind::comment) continue;
            if (out.sections.empty()) select(".text");
            auto &section = out.sections[current];
            auto &bytes = section.bytes;
            if (s.kind == Statement::Kind::label) out.symbols.push_back({s.name, current, bytes.size(), false});
            else if (s.kind == Statement::Kind::align) {
                size_t n = size_t(detail::integer(s.name)), padding = (n - bytes.size() % n) % n;
                room(padding); bytes.insert(bytes.end(), padding, 0); section.alignment = std::max(section.alignment, n);
            } else if (s.kind == Statement::Kind::ascii) {
                room(s.name.size()); bytes.insert(bytes.end(), s.name.begin(), s.name.end());
            } else if (s.kind == Statement::Kind::encoder || s.kind == Statement::Kind::form) {
                auto v = detail::encode(s, t);
                room(v.bytes.size());
                for (const auto &r : v.relocations) {
                    const auto &desc = t.spec().get()->relocs[r.index];
                    auto ref = v.symbols.find(r.parameter);
                    if (r.offset > std::numeric_limits<size_t>::max() - bytes.size())
                        throw Error("relocation offset overflow");
                    out.relocations.push_back({current, bytes.size() + r.offset, r.size, r.name, r.parameter,
                        ref == v.symbols.end() ? std::string{} : ref->second,
                        r.value, r.bit_offset, r.bit_width, r.endian, desc.pcrel});
                }
                bytes.insert(bytes.end(), v.bytes.begin(), v.bytes.end());
            } else if (s.kind == Statement::Kind::instruction) {
                throw Error("native assembly text cannot be encoded; use encode or form with explicit parameter bindings");
            } else {
                size_t width = s.kind == Statement::Kind::byte ? 1 : s.kind == Statement::Kind::word ? 2 : s.kind == Statement::Kind::long_ ? 4 : 8;
                for (const auto &arg : s.args) {
                    int64_t value = detail::integer(arg);
                    if (width < 8 && (value < -(int64_t(1) << (width * 8 - 1)) ||
                        (value >= 0 && uint64_t(value) >= (uint64_t(1) << (width * 8))))) throw Error("data value does not fit directive");
                    room(width);
                    for (size_t i = 0; i < width; ++i) bytes.push_back(uint8_t(uint64_t(value) >> ((little ? i : width - i - 1) * 8)));
                }
            }
        } catch (const Error &e) { throw Error(e.what(), s.location); }
    }
    for (auto &s : out.symbols) s.global = globals.erase(s.name) != 0;
    if (!globals.empty()) throw Error("global symbol is not defined: " + *globals.begin());
    return out;
}
inline std::string read_file(const std::filesystem::path &path, Limits limits = {}) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw Error("cannot open input: " + path.string());
    std::string out;
    char chunk[8192];
    while (in.read(chunk, sizeof chunk) || in.gcount()) {
        auto n = size_t(in.gcount());
        if (out.size() > limits.source_bytes || n > limits.source_bytes - out.size()) throw Error("source byte limit exceeded");
        out.append(chunk, n);
    }
    if (!in.eof()) throw Error("read failed: " + path.string());
    return out;
}
inline void compile_file(const std::filesystem::path &input, const std::filesystem::path &output,
                         const Target &t, Limits limits = {}) {
    if (std::filesystem::weakly_canonical(input) == std::filesystem::weakly_canonical(output) ||
        (std::filesystem::exists(output) && std::filesystem::equivalent(input, output))) throw Error("input and output must differ");
    auto result = render(parse(read_file(input, limits), input.string(), limits), t, limits);
    std::ofstream out(output, std::ios::binary);
    if (!out) throw Error("cannot open output: " + output.string());
    out.write(result.data(), std::streamsize(result.size())); out.close();
    if (!out) throw Error("write failed: " + output.string());
}
} // namespace mair
