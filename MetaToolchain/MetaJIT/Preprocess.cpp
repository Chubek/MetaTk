#include "Preprocess.hpp"
#include "Runtime.hpp"
#include <GPPX.hpp>
#include <MetaIL.h>

#include <algorithm>
#include <bitset>
#include <deque>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

namespace metajit {
namespace {
using Bindings = std::vector<std::pair<std::string, std::string>>;
constexpr std::size_t max_source = 8 * 1024 * 1024;
constexpr std::size_t max_slots = 1024;

[[noreturn]] void fail(const std::string &message) {
    throw std::runtime_error("MetaJIT: " + message);
}
bool identifier(std::string_view name) {
    auto alpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    if (name.empty() || !alpha(name.front())) return false;
    return std::all_of(name.begin(), name.end(), [&](char c) { return alpha(c) || (c >= '0' && c <= '9'); });
}
std::string trim(std::string_view s) {
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == s.npos) return {};
    auto last = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(first, last - first + 1));
}
std::pair<std::string, std::string> split(std::string_view s) {
    auto text = trim(s);
    auto pos = text.find_first_of(" \t");
    return {text.substr(0, pos), pos == text.npos ? "" : trim(text.substr(pos))};
}

struct Patch { std::size_t offset, binding; };
struct Image {
    std::vector<std::uint8_t> bytes;
    std::vector<Entry> entries;
    std::vector<Patch> patches;

    void emit(std::initializer_list<std::uint8_t> data) {
        if (data.size() > 1024 * 1024 - bytes.size()) fail("native code exceeds 1 MiB limit");
        bytes.insert(bytes.end(), data.begin(), data.end());
    }
    void word(std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) emit({static_cast<std::uint8_t>(value >> (8 * i))});
    }
    void immediate(std::uint64_t value) {
        emit({0x48, 0xb8}); // movabs rax, imm64
        for (unsigned i = 0; i < 8; ++i) emit({static_cast<std::uint8_t>(value >> (8 * i))});
    }
    std::size_t jump(std::initializer_list<std::uint8_t> opcode) {
        emit(opcode);
        const auto offset = bytes.size();
        word(0);
        return offset;
    }
    void resolve(std::size_t offset, std::size_t target) {
        const auto distance = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(offset + 4);
        if (distance < INT32_MIN || distance > INT32_MAX) fail("branch displacement overflow");
        auto value = static_cast<std::uint32_t>(distance);
        for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8 * i));
    }
};

// Stack slots keep assignments and control-flow joins explicit. No registers
// survive expression evaluation except RBP/RSP; temporary pushes are balanced.
class Lower {
    Image &image_;
    const metail_function &function_;
    const Bindings &bindings_;
    std::map<std::string, std::size_t> slots_, labels_;
    std::map<std::string, std::size_t> bindings_by_name_;
    std::vector<std::pair<std::size_t, std::string>> branches_;
    std::vector<std::size_t> positions_;

    [[noreturn]] void error(const std::string &s) const { fail("function " + std::string(function_.name) + ": " + s); }
    void slot(const std::string &name) {
        if (bindings_by_name_.count(name)) error("binding shadows local or parameter: " + name);
        if (!slots_.count(name)) {
            if (slots_.size() == max_slots) error("more than 1024 locals/parameters");
            const auto index = slots_.size();
            slots_.emplace(name, index);
        }
    }
    void inspect(const metail_expr &e, std::bitset<max_slots> &reads) const {
        if (e.kind == METAIL_CALL) error("calls are not supported by the native backend: " + std::string(e.name));
        if (e.kind == METAIL_VAR) {
            auto it = slots_.find(e.name);
            if (it != slots_.end()) reads.set(it->second);
            else if (!bindings_by_name_.count(e.name)) error("undefined variable: " + std::string(e.name));
        }
        for (std::size_t i = 0; i < e.arg_count; ++i) inspect(*e.args[i], reads);
    }
    void validate() {
        const auto count = function_.statement_count;
        if (!count || count > 4096) error("expected 1..4096 statements");
        if (function_.param_count > 6) error("at most six int64 parameters are supported");
        for (std::size_t i = 0; i < bindings_.size(); ++i) bindings_by_name_.emplace(bindings_[i].first, i);
        for (std::size_t i = 0; i < function_.param_count; ++i) slot(function_.params[i]);
        std::bitset<max_slots> parameters;
        for (std::size_t i = 0; i < function_.param_count; ++i) parameters.set(slots_.at(function_.params[i]));
        for (std::size_t i = 0; i < count; ++i) {
            const auto &s = function_.statements[i];
            if (s.kind == METAIL_LET) slot(s.name);
            if (s.kind == METAIL_LABEL) labels_.emplace(s.name, i);
            // MetaIL does not yet specify executable semantics for assumptions.
            if (s.kind == METAIL_ASSUME) error("assume has no specified execution contract; use br instead");
        }
        std::vector<std::bitset<max_slots>> reads(count), incoming(count);
        std::vector<std::vector<std::size_t>> successors(count);
        for (std::size_t i = 0; i < count; ++i) {
            const auto &s = function_.statements[i];
            if (s.expr) inspect(*s.expr, reads[i]);
            if (s.kind == METAIL_GOTO) successors[i].push_back(labels_.at(s.target));
            else if (s.kind == METAIL_BRANCH) successors[i] = {labels_.at(s.yes), labels_.at(s.no)};
            else if (s.kind != METAIL_RETURN) successors[i].push_back(i + 1);
        }
        // Must-definition analysis: intersect facts from every reachable edge,
        // including back edges and the synthetic parameter-only entry edge.
        std::vector<bool> reached(count, false), queued(count, false);
        incoming[0] = parameters;
        reached[0] = queued[0] = true;
        std::deque<std::size_t> work{0};
        while (!work.empty()) {
            const auto i = work.front(); work.pop_front(); queued[i] = false;
            auto outgoing = incoming[i];
            const auto &s = function_.statements[i];
            if (s.kind == METAIL_LET) outgoing.set(slots_.at(s.name));
            for (auto next : successors[i]) {
                if (next == count) error("reachable path falls off the function without return");
                auto merged = reached[next] ? incoming[next] & outgoing : outgoing;
                if (!reached[next] || merged != incoming[next]) {
                    incoming[next] = merged;
                    reached[next] = true;
                    if (!queued[next]) { work.push_back(next); queued[next] = true; }
                }
            }
        }
        for (std::size_t i = 0; i < count; ++i) {
            if (!reached[i]) continue;
            auto missing = reads[i] & ~incoming[i];
            for (const auto &[name, index] : slots_)
                if (missing.test(index)) error("possibly uninitialized variable " + name + " at statement " + std::to_string(i + 1));
        }
    }
    void displacement(std::size_t slot_index) { image_.word(0U - static_cast<std::uint32_t>(8 * (slot_index + 1))); }
    void boolean(std::uint8_t condition) {
        image_.emit({0x0f, condition, 0xc0, 0x0f, 0xb6, 0xc0}); // setcc al; movzx eax,al
    }
    void expression(const metail_expr &e) {
        if (e.kind == METAIL_INT) { image_.immediate(static_cast<std::uint64_t>(e.value)); return; }
        if (e.kind == METAIL_VAR) {
            auto bound = bindings_by_name_.find(e.name);
            if (bound != bindings_by_name_.end()) {
                image_.immediate(0);
                image_.patches.push_back({image_.bytes.size() - 8, bound->second});
            } else {
                image_.emit({0x48, 0x8b, 0x85}); displacement(slots_.at(e.name)); // mov rax,[rbp-disp32]
            }
            return;
        }
        const std::string op(e.op);
        expression(*e.args[0]);
        if (op == "select") {
            image_.emit({0x48, 0x85, 0xc0});
            const auto no = image_.jump({0x0f, 0x84});
            expression(*e.args[1]);
            const auto end = image_.jump({0xe9});
            image_.resolve(no, image_.bytes.size());
            expression(*e.args[2]);
            image_.resolve(end, image_.bytes.size());
            return;
        }
        if (op == "neg") { image_.emit({0x48, 0xf7, 0xd8}); return; }
        if (op == "bnot") { image_.emit({0x48, 0xf7, 0xd0}); return; }
        if (op == "lnot") { image_.emit({0x48, 0x85, 0xc0}); boolean(0x94); return; }
        image_.emit({0x50}); // push lhs
        expression(*e.args[1]);
        image_.emit({0x48, 0x89, 0xc1, 0x58}); // mov rcx,rax; pop rax
        if (op == "add") image_.emit({0x48, 0x01, 0xc8});
        else if (op == "sub") image_.emit({0x48, 0x29, 0xc8});
        else if (op == "mul") image_.emit({0x48, 0x0f, 0xaf, 0xc1});
        else if (op == "band") image_.emit({0x48, 0x21, 0xc8});
        else if (op == "bor") image_.emit({0x48, 0x09, 0xc8});
        else if (op == "bxor") image_.emit({0x48, 0x31, 0xc8});
        else if (op == "div" || op == "mod") {
            image_.emit({0x48, 0x99, 0x48, 0xf7, 0xf9}); // cqo; idiv rcx (preserves divide traps)
            if (op == "mod") image_.emit({0x48, 0x89, 0xd0});
        } else if (op == "shl" || op == "shr") {
            // Unsigned comparison rejects negative counts too; never mask them.
            image_.emit({0x48, 0x83, 0xf9, 0x3f, 0x76, 0x02, 0x0f, 0x0b});
            image_.emit({0x48, 0xd3, static_cast<std::uint8_t>(op == "shl" ? 0xe0 : 0xf8)});
        } else if (op == "land" || op == "lor") {
            // MetaIL logical operators are eager; only select is lazy.
            image_.emit({0x48, 0x85, 0xc0}); boolean(0x95);
            image_.emit({0x48, 0x85, 0xc9, 0x0f, 0x95, 0xc1, 0x0f, 0xb6, 0xc9});
            image_.emit({0x48, static_cast<std::uint8_t>(op == "land" ? 0x21 : 0x09), 0xc8});
        } else {
            static const std::map<std::string, std::uint8_t> conditions{
                {"eq", 0x94}, {"ne", 0x95}, {"lt", 0x9c}, {"le", 0x9e}, {"gt", 0x9f}, {"ge", 0x9d}};
            auto cc = conditions.find(op);
            if (cc == conditions.end()) error("unsupported operator: " + op);
            image_.emit({0x48, 0x39, 0xc8}); boolean(cc->second);
        }
    }
public:
    Lower(Image &image, const metail_function &function, const Bindings &bindings)
        : image_(image), function_(function), bindings_(bindings) {}
    void run() {
        validate();
        // Readable prefix accommodates instrumented indirect-call probes. ENDBR64
        // permits entry on CET/IBT hosts and is a NOP on older x86-64 machines.
        image_.emit({0, 0, 0, 0, 0, 0, 0, 0});
        image_.entries.push_back({function_.name, image_.bytes.size(), function_.param_count});
        image_.emit({0xf3, 0x0f, 0x1e, 0xfa, 0x55, 0x48, 0x89, 0xe5, 0x48, 0x81, 0xec});
        image_.word(static_cast<std::uint32_t>((slots_.size() * 8 + 15) & ~std::size_t(15)));
        constexpr std::uint8_t modrm[] = {0xbd, 0xb5, 0x95, 0x8d, 0x85, 0x8d};
        for (std::size_t i = 0; i < function_.param_count; ++i) {
            image_.emit({static_cast<std::uint8_t>(i < 4 ? 0x48 : 0x4c), 0x89, modrm[i]});
            displacement(slots_.at(function_.params[i]));
        }
        for (std::size_t i = 0; i < function_.statement_count; ++i) {
            positions_.push_back(image_.bytes.size());
            const auto &s = function_.statements[i];
            if (s.expr) expression(*s.expr);
            switch (s.kind) {
            case METAIL_LET:
                image_.emit({0x48, 0x89, 0x85}); displacement(slots_.at(s.name)); break;
            case METAIL_RETURN: image_.emit({0xc9, 0xc3}); break; // leave; ret
            case METAIL_GOTO: branches_.push_back({image_.jump({0xe9}), s.target}); break;
            case METAIL_BRANCH:
                image_.emit({0x48, 0x85, 0xc0});
                branches_.push_back({image_.jump({0x0f, 0x85}), s.yes});
                branches_.push_back({image_.jump({0xe9}), s.no}); break;
            case METAIL_LABEL: case METAIL_EVAL: break;
            default: error("unsupported statement");
            }
        }
        image_.emit({0x0f, 0x0b}); // Defensive fence after the final unreachable instruction.
        for (const auto &[offset, name] : branches_) image_.resolve(offset, positions_.at(labels_.at(name)));
    }
};

std::string generate(metail_context *context, const std::string &source,
                     const std::string &name, const Bindings &bindings) {
    metail_module *raw = nullptr;
    const auto status = metail_parse(context, source.c_str(), name.c_str(), &raw);
    std::unique_ptr<metail_module, decltype(&metail_module_free)> module(raw, metail_module_free);
    if (status != METAIL_OK) fail(metail_last_error(context));
    if (module->function_count > 128) fail("more than 128 functions in one JIT block");
    Image image;
    for (std::size_t i = 0; i < module->function_count; ++i) Lower(image, module->functions[i], bindings).run();
    std::set<std::size_t> used;
    for (const auto &patch : image.patches) used.insert(patch.binding);
    for (std::size_t i = 0; i < bindings.size(); ++i)
        if (!used.count(i)) fail("unused binding: " + bindings[i].first);
    std::ostringstream out;
    out << "[&]() -> metajit::Executable {\n";
    for (std::size_t i = 0; i < bindings.size(); ++i)
        out << "const std::uint64_t metajit_detail_value_" << i << " = static_cast<std::uint64_t>(("
            << bindings[i].second << "));\n";
    out << "metajit::CodeBuffer metajit_detail_buffer;\n";
    // Bounded initializer lists keep generated C++ practical for large functions.
    for (std::size_t start = 0; start < image.bytes.size(); start += 256) {
        out << "metajit_detail_buffer.append({";
        for (auto i = start; i < std::min(start + 256, image.bytes.size()); ++i) {
            if (i != start) out << ',';
            out << static_cast<unsigned>(image.bytes[i]);
        }
        out << "});\n";
    }
    for (const auto &p : image.patches)
        out << "metajit_detail_buffer.patch64(" << p.offset << ", metajit_detail_value_" << p.binding << ");\n";
    for (const auto &e : image.entries)
        out << "metajit_detail_buffer.entry(\"" << e.name << "\", " << e.offset << ", " << e.arity << ");\n";
    out << "return metajit_detail_buffer.finalize();\n}()";
    return out.str();
}

class Session {
    std::unique_ptr<metail_context, decltype(&metail_destroy)> context_{nullptr, metail_destroy};
    gppx::Engine engine_;
    std::string target_, body_, name_;
    Bindings bindings_;
public:
    Session(const Preprocess::Options &options, std::string name) : name_(std::move(name)) {
        if (name_.find('\0') != name_.npos || options.metail_home.find('\0') != options.metail_home.npos)
            fail("embedded NUL in path");
        char error[2048]{};
        context_.reset(metail_create(options.metail_home.empty() ? nullptr : options.metail_home.c_str(), error, sizeof error));
        if (!context_) fail(error);
        // C/C++ #directives are ordinary text. Explicit macros avoid accidentally
        // rewriting host-language identifiers.
        engine_.set("prefix", "@");
        engine_.set("macros", "dollar");
        engine_.set("strict", "true");
        for (const auto &path : options.include_paths) engine_.includePath(path);
        for (const auto &[key, value] : options.definitions) engine_.define(key, value);
        engine_.registerDirective("jit_begin", [this](std::string_view arg) {
            if (!target_.empty()) fail("nested jit_begin");
            auto name = trim(arg);
            // Keywords are rejected by the C++ compiler; require an identifier,
            // not an arbitrary expression capable of injecting generated syntax.
            if (!identifier(name) || name.rfind("metajit_detail_", 0) == 0)
                fail("jit_begin expects a C++ variable identifier");
            target_ = name; body_.clear(); bindings_.clear();
            return std::string{};
        });
        engine_.registerDirective("jit_bind", [this](std::string_view arg) {
            if (target_.empty()) fail("jit_bind outside JIT block");
            auto [name, expression] = split(arg);
            if (!identifier(name) || expression.empty()) fail("jit_bind expects NAME C++-EXPRESSION");
            if (bindings_.size() >= 256) fail("more than 256 bindings");
            for (const auto &b : bindings_) if (b.first == name) fail("duplicate binding: " + name);
            bindings_.emplace_back(name, expression);
            return std::string{};
        });
        engine_.registerDirective("jit_end", [this](std::string_view arg) {
            if (target_.empty()) fail("jit_end without jit_begin");
            if (!trim(arg).empty()) fail("jit_end takes no arguments");
            auto cpp = "auto " + target_ + " = " + generate(context_.get(), body_, name_ + ":" + target_, bindings_) + ";\n";
            target_.clear(); body_.clear(); bindings_.clear();
            return cpp;
        });
        engine_.registerRewriter([this](std::string_view line) {
            if (line.find('\0') != line.npos) fail("embedded NUL in source");
            if (target_.empty()) return std::string(line);
            if (line.size() > max_source - body_.size()) fail("JIT block exceeds 8 MiB limit");
            body_.append(line);
            return std::string{};
        });
    }
    std::string process(std::string_view source) {
        if (source.size() > max_source || source.find('\0') != source.npos) fail("source exceeds 8 MiB or contains NUL");
        auto result = engine_.process(source, name_);
        if (!target_.empty()) fail(name_ + ": missing jit_end for " + target_);
        return result;
    }
};
} // namespace

std::string Preprocess::process(std::string_view source, const std::string &name) const {
    return Session(options_, name).process(source);
}
std::string Preprocess::file(const std::filesystem::path &path) const {
    std::ifstream input(path, std::ios::binary);
    if (!input) fail("cannot open " + path.string());
    std::string source;
    char buffer[8192];
    while (input.read(buffer, sizeof buffer) || input.gcount()) {
        const auto size = static_cast<std::size_t>(input.gcount());
        if (size > max_source - source.size()) fail("source exceeds 8 MiB");
        source.append(buffer, size);
    }
    if (input.bad()) fail("cannot read " + path.string());
    return process(source, std::filesystem::absolute(path).string());
}
} // namespace metajit
