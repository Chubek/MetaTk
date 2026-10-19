#include "MetaArch.hpp"
#include "DSLtk.hpp"

#include <charconv>
#include <limits>
#include <set>

namespace metaarch {
namespace {
struct Diags {
    DsymDiags d;
    Diags() { dsym_diags_init(&d); }
    ~Diags() { dsym_diags_free(&d); }
    std::vector<Diagnostic> copy() const {
        std::vector<Diagnostic> out;
        for (size_t i = 0; i < d.count; ++i) {
            const auto &v = d.items[i];
            out.push_back({v.severity, v.file ? v.file : "", v.path ? v.path : "",
                           v.msg ? v.msg : "", v.line, v.col});
        }
        return out;
    }
};
struct Buffer {
    DsymBuf b{};
    ~Buffer() { dsym_buf_free(&b); }
};
using Arena = std::unique_ptr<DsymArena, decltype(&dsym_arena_free)>;

Error failure(std::string message, const std::string &path = "", size_t line = 0,
              size_t column = 0) {
    return {{{DSYM_ERROR, path, "", std::move(message), line, column}}};
}
Error parse_error(std::string_view source, const std::string &path, const dsl::ParseError &e) {
    size_t line = 1, col = 1;
    for (size_t i = 0; i < e.pos && i < source.size(); ++i)
        if (source[i] == '\n') { ++line; col = 1; } else ++col;
    std::string text = "expected ";
    for (const auto &label : e.expected) {
        if (text.size() > 9) text += " or ";
        text += label;
    }
    return failure(text, path, line, col);
}

bool alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool digit(char c) { return c >= '0' && c <= '9'; }
bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// Common lexical parsers are DSLtk combinators, shared by both languages.
const auto identifier_chars = dsl::satisfy([](char c) {
    return alpha(c) || digit(c) || c == '.' || c == '-';
}, "identifier character");
const auto identifier = dsl::parser([](dsl::ParsecInput &in) -> dsl::ExpectedResult<std::string> {
    size_t start = in.pos;
    if (!dsl::satisfy(alpha, "identifier")(in))
        return dsl::fail_expected<std::string>(in, "identifier");
    (*identifier_chars)(in);
    return std::string(in.get_span(start));
});
const auto whitespace = *dsl::satisfy(space, "whitespace");
void skip(dsl::ParsecInput &in) {
    for (;;) {
        whitespace(in);
        if (in.eof() || in.peek() != '#') break;
        (*dsl::satisfy([](char c) { return c != '\n'; }, "comment"))(in);
    }
}
bool token(dsl::ParsecInput &in, char c) {
    skip(in);
    return bool(dsl::ch(c)(in));
}

// Signed decimal/0x/0b/0o, checked without strtoll's locale/overflow ambiguities.
const auto integer = dsl::parser([](dsl::ParsecInput &in) -> dsl::ExpectedResult<int64_t> {
    size_t start = in.pos;
    auto sign = dsl::optional(dsl::ch('-') | dsl::ch('+'))(in);
    bool negative = *sign && **sign == '-';
    int base = 10;
    if (in.source.substr(in.pos, 2) == "0x") { base = 16; in.pos += 2; }
    else if (in.source.substr(in.pos, 2) == "0b") { base = 2; in.pos += 2; }
    else if (in.source.substr(in.pos, 2) == "0o") { base = 8; in.pos += 2; }
    size_t digits = in.pos;
    (*dsl::satisfy([](char c) { return digit(c) || (c >= 'a' && c <= 'f') ||
                                                      (c >= 'A' && c <= 'F'); }, "digit"))(in);
    uint64_t value = 0;
    auto text = in.source.substr(digits, in.pos - digits);
    auto r = std::from_chars(text.data(), text.data() + text.size(), value, base);
    uint64_t limit = uint64_t(std::numeric_limits<int64_t>::max()) + (negative ? 1 : 0);
    if (text.empty() || r.ec != std::errc{} || r.ptr != text.data() + text.size() || value > limit)
        return dsl::ExpectedResult<int64_t>::failure(start, dsl::ParseFailureKind::Committed,
                                                    {"signed 64-bit integer"});
    if (negative && value == limit) return std::numeric_limits<int64_t>::min();
    return negative ? -int64_t(value) : int64_t(value);
});

// Schema-shaped DSL with declaration sugar. All semantics go through the C binder.
class ArchitectureParser {
    DsymArena *arena_;
    size_t max_depth_;
    std::vector<size_t> line_starts_{0};
    template<class T> static dsl::ExpectedResult<T> fail(const dsl::ParsecInput &in, std::string s) {
        return dsl::fail_expected<T>(in, std::move(s), dsl::ParseFailureKind::Committed);
    }
    dsl::ExpectedResult<DsymNode *> value(dsl::ParsecInput &in, size_t depth) {
        skip(in);
        size_t start = in.pos;
        if (depth > max_depth_) return fail<DsymNode *>(in, "shallower nesting (depth limit)");
        DsymNode *node = nullptr;
        if (in.peek() == '{') {
            auto r = object(in, depth + 1, false);
            if (!r) return r;
            node = *r;
        } else if (token(in, '[')) {
            node = dsym_node_seq(arena_);
            if (!node) return fail<DsymNode *>(in, "available memory");
            if (!token(in, ']')) for (;;) {
                auto r = value(in, depth + 1);
                if (!r) return r;
                if (!dsym_seq_push(arena_, node, *r)) return fail<DsymNode *>(in, "available memory");
                if (token(in, ']')) break;
                if (!token(in, ',')) return fail<DsymNode *>(in, "',' or ']'");
            }
        } else if (in.peek() == '"') {
            // DSLtk handles the quoted production; JSON codec decodes escapes/Unicode.
            const auto quoted = dsl::parser([](dsl::ParsecInput &s) -> dsl::ExpectedResult<std::string_view> {
                size_t begin = s.pos;
                dsl::ch('"')(s);
                auto ordinary = dsl::satisfy([](char c) { return c != '"' && c != '\\' &&
                                                                    (unsigned char)c >= 32; }, "string character");
                auto escaped = dsl::parser([](dsl::ParsecInput &p) -> dsl::ExpectedResult<char> {
                    if (!dsl::ch('\\')(p)) return dsl::fail_expected<char>(p, "escape");
                    return dsl::satisfy([](char c) { return (unsigned char)c >= 32; }, "escape code")(p);
                });
                (*(ordinary | escaped))(s);
                if (!dsl::ch('"')(s)) return dsl::fail_expected<std::string_view>(s, "closing quote");
                return s.get_span(begin);
            });
            auto r = quoted(in);
            if (!r) return fail<DsymNode *>(in, "valid quoted string");
            Diags d;
            node = dsym_json_read(arena_, (*r).data(), (*r).size(), "<string>", &d.d);
            if (!node) return fail<DsymNode *>(in, "valid JSON string escapes");
        } else if (digit(in.peek()) || in.peek() == '-' || in.peek() == '+') {
            auto r = integer(in);
            if (!r) return dsl::ExpectedResult<DsymNode *>::failure(r.error.pos, r.error.kind, r.error.expected);
            node = dsym_node_int(arena_, *r);
        } else {
            auto r = identifier(in);
            if (!r) return fail<DsymNode *>(in, "value");
            if (*r == "true" || *r == "false") node = dsym_node_bool(arena_, *r == "true");
            else if (*r == "null") node = dsym_node_null(arena_);
            else node = dsym_node_str(arena_, (*r).c_str());
        }
        if (!node) return fail<DsymNode *>(in, "available memory");
        auto line = std::upper_bound(line_starts_.begin(), line_starts_.end(), start);
        node->line = uint32_t(line - line_starts_.begin());
        node->col = uint32_t(start - *(line - 1) + 1);
        return node;
    }
    dsl::ExpectedResult<DsymNode *> object(dsl::ParsecInput &in, size_t depth, bool declarations) {
        if (depth > max_depth_) return fail<DsymNode *>(in, "shallower nesting (depth limit)");
        if (!token(in, '{')) return fail<DsymNode *>(in, "'{'");
        auto *map = dsym_node_map(arena_);
        if (!map) return fail<DsymNode *>(in, "available memory");
        const std::map<std::string, std::string> plurals = {
            {"target", "targets"}, {"register", "registers"}, {"feature", "features"},
            {"mnemonic", "mnemonics"}, {"encoder", "encoders"}, {"form", "forms"},
            {"syntax", "syntaxes"}, {"section", "sections"}, {"reloc", "relocs"}};
        while (!token(in, '}')) {
            auto key = identifier(in);
            if (!key) return fail<DsymNode *>(in, "field or declaration");
            if (token(in, '=')) {
                if (dsym_map_get(map, (*key).c_str())) return fail<DsymNode *>(in, "unique field");
                auto r = value(in, depth + 1);
                if (!r) return r;
                if (!dsym_map_put(arena_, map, (*key).data(), (*key).size(), *r))
                    return fail<DsymNode *>(in, "available memory");
                if (!token(in, ';')) return fail<DsymNode *>(in, "';'");
            } else if (declarations && plurals.count(*key)) {
                std::string name;
                if (*key != "target" && *key != "form") {
                    skip(in);
                    auto r = identifier(in);
                    if (!r) return fail<DsymNode *>(in, "declaration name");
                    name = *r;
                }
                auto r = object(in, depth + 1, false);
                if (!r) return r;
                if (!name.empty()) {
                    if (dsym_map_get(*r, "name")) return fail<DsymNode *>(in, "name only in declaration header");
                    if (!dsym_map_put_ref(arena_, *r, "name", dsym_node_str(arena_, name.c_str())))
                        return fail<DsymNode *>(in, "available memory");
                }
                const auto &plural = plurals.at(*key);
                auto *seq = dsym_map_get(map, plural.c_str());
                if (!seq) {
                    seq = dsym_node_seq(arena_);
                    if (!seq || !dsym_map_put(arena_, map, plural.data(), plural.size(), seq))
                        return fail<DsymNode *>(in, "available memory");
                }
                if (seq->kind != DSYM_NODE_SEQ) return fail<DsymNode *>(in, "declaration list");
                if (!dsym_seq_push(arena_, seq, *r)) return fail<DsymNode *>(in, "available memory");
            } else return fail<DsymNode *>(in, "'=' or architecture declaration");
            skip(in);
        }
        return map;
    }
public:
    ArchitectureParser(DsymArena *a, size_t depth, std::string_view source) : arena_(a), max_depth_(depth) {
        for (size_t i = 0; i < source.size(); ++i)
            if (source[i] == '\n') line_starts_.push_back(i + 1);
    }
    dsl::Parser<DsymNode *> parser() {
        return dsl::parser([this](dsl::ParsecInput &in) -> dsl::ExpectedResult<DsymNode *> {
            skip(in);
            auto keyword = identifier(in);
            if (!keyword || *keyword != "architecture") return fail<DsymNode *>(in, "'architecture'");
            auto r = object(in, 0, true);
            skip(in);
            return r;
        });
    }
};

struct Instruction {
    std::string encoder;
    std::vector<Binding> named;
    std::vector<Operand> positional;
    size_t offset;
};
const auto assembly_line = dsl::parser([](dsl::ParsecInput &in) -> dsl::ExpectedResult<Instruction> {
    auto horizontal = *dsl::satisfy([](char c) { return c == ' ' || c == '\t' || c == '\r'; }, "space");
    horizontal(in);
    Instruction out{};
    out.offset = in.pos;
    if (in.eof() || in.peek() == '#' || in.peek() == ';') {
        (*dsl::satisfy([](char) { return true; }, "comment"))(in);
        return out;
    }
    auto name = identifier(in);
    if (!name) return dsl::fail_expected<Instruction>(in, "encoder name");
    out.encoder = *name;
    if (!in.eof() && in.peek() != '#' && in.peek() != ';' && !space(in.peek()))
        return dsl::fail_expected<Instruction>(in, "space after encoder name");
    horizontal(in);
    bool named = false, positional = false;
    while (!in.eof() && in.peek() != '#' && in.peek() != ';') {
        size_t start = in.pos;
        auto key = identifier(in);
        horizontal(in);
        std::string parameter;
        if (key && dsl::ch('=')(in)) { parameter = *key; named = true; }
        else { in.pos = start; positional = true; }
        if (named && positional) return dsl::fail_expected<Instruction>(in, "all named or all positional operands");
        horizontal(in);
        Operand operand;
        if (digit(in.peek()) || in.peek() == '-' || in.peek() == '+') {
            auto r = integer(in);
            if (!r) return dsl::ExpectedResult<Instruction>::failure(r.error.pos, r.error.kind, r.error.expected);
            operand = *r;
        } else {
            auto r = identifier(in);
            if (!r) return dsl::fail_expected<Instruction>(in, "register or integer");
            operand = *r;
        }
        if (named) out.named.push_back({parameter, std::move(operand)});
        else out.positional.push_back(std::move(operand));
        horizontal(in);
        if (in.eof() || in.peek() == '#' || in.peek() == ';') break;
        if (!dsl::ch(',')(in)) return dsl::fail_expected<Instruction>(in, "',' between operands");
        horizontal(in);
        if (in.eof() || in.peek() == '#' || in.peek() == ';')
            return dsl::fail_expected<Instruction>(in, "operand after comma");
    }
    if (!in.eof()) (*dsl::satisfy([](char) { return true; }, "comment"))(in);
    return out;
});
} // namespace

Result<Spec> Spec::load(const std::string &path, uint32_t flags) {
    if (path.ends_with(".march")) { ParseOptions o; o.load_flags = flags; return load_dsl(path, o); }
    Diags d;
    auto *s = dsym_spec_load_file(path.c_str(), DSYM_FORMAT_AUTO, flags, &d.d);
    if (!s) return Error{d.copy()};
    Spec out(s); out.diagnostics_ = d.copy(); return out;
}
Result<Spec> Spec::parse(std::string_view source, DsymFormat fmt, const std::string &path, uint32_t flags) {
    Diags d;
    auto *s = dsym_spec_load_buffer(source.data(), source.size(), fmt, path.c_str(), flags, &d.d);
    if (!s) return Error{d.copy()};
    Spec out(s); out.diagnostics_ = d.copy(); return out;
}
Result<Spec> Spec::load_dsl(const std::string &path, ParseOptions options) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return failure("cannot open architecture DSL", path);
    std::string source;
    char chunk[4096];
    while (f.read(chunk, sizeof chunk) || f.gcount()) {
        size_t n = size_t(f.gcount());
        if (source.size() > options.max_source_bytes || n > options.max_source_bytes - source.size())
            return failure("source byte limit exceeded", path);
        source.append(chunk, n);
    }
    if (f.bad()) return failure("cannot read architecture DSL", path);
    return parse_dsl(source, path, options);
}
Result<Spec> Spec::parse_dsl(std::string_view source, const std::string &path, ParseOptions options) {
    if (source.size() > options.max_source_bytes) return failure("source byte limit exceeded", path);
    Arena arena(dsym_arena_new(), dsym_arena_free);
    if (!arena) return failure("out of memory", path);
    ArchitectureParser grammar(arena.get(), options.max_depth, source);
    auto r = dsl::run_parser(grammar.parser(), source);
    if (!r.value) return parse_error(source, path, r.error);
    auto *root = *r.value;
    AssemblyBindings bindings;
    if (auto *assembly = dsym_map_get(root, "assembly")) {
        if (assembly->kind != DSYM_NODE_MAP) return failure("assembly must be a map", path);
        for (size_t i = 0; i < assembly->v.map.count; ++i) {
            const auto &p = assembly->v.map.items[i];
            if (p.value->kind != DSYM_NODE_SEQ) return failure("assembly binding must be a parameter list", path);
            auto &params = bindings[p.key];
            std::set<std::string> unique;
            for (size_t j = 0; j < p.value->v.seq.count; ++j) {
                auto *v = p.value->v.seq.items[j];
                if (v->kind != DSYM_NODE_STR) return failure("assembly parameter must be a string", path);
                std::string param(v->v.str.ptr, v->v.str.len);
                if (!unique.insert(param).second) return failure("duplicate assembly parameter", path);
                params.push_back(std::move(param));
            }
        }
        for (size_t i = 0; i < root->v.map.count; ++i)
            if (std::string_view(root->v.map.items[i].key) == "assembly") {
                for (size_t j = i + 1; j < root->v.map.count; ++j) root->v.map.items[j - 1] = root->v.map.items[j];
                --root->v.map.count; break;
            }
    }
    Diags d;
    auto *s = dsym_spec_from_tree(root, path.c_str(), options.load_flags, &d.d);
    if (!s) return Error{d.copy()};
    Spec out(s);
    out.diagnostics_ = d.copy();
    for (const auto &[encoder, params] : bindings) {
        auto index = dsym_spec_find_encoder(s, encoder.c_str());
        if (index < 0) return failure("assembly references unknown encoder: " + encoder, path);
        auto *program = s->encoders[index].program;
        if (!program) return failure("assembly bindings require compiled encoders", path);
        if (params.size() != program->param_count) return failure("assembly binding must cover every parameter: " + encoder, path);
        for (const auto &p : params)
            if (dsym_program_param_index(program, p.c_str()) < 0)
                return failure("unknown assembly parameter: " + p, path);
    }
    out.bindings_ = std::move(bindings);
    return out;
}
const DsymRegister *Spec::find_register(const std::string &name) const {
    if (!spec_) return nullptr;
    int32_t index = dsym_spec_find_register(spec_.get(), name.c_str());
    return index < 0 ? nullptr : &spec_->registers[index];
}
Result<std::string> Spec::write(DsymFormat fmt, uint32_t flags) const {
    if (!spec_) return failure("empty spec (moved from)");
    Diags d; Buffer b;
    if (!dsym_spec_write(spec_.get(), fmt, flags, &b.b, &d.d)) return Error{d.copy()};
    return std::string(b.b.data ? b.b.data : "", b.b.len);
}
Result<Encoded> Spec::encode(const std::string &encoder, std::span<const Binding> operands, size_t target) const {
    if (!spec_) return failure("empty spec (moved from)");
    if (target >= spec_->target_count) return failure("target index out of range");
    std::set<std::string> unique;
    std::vector<const char *> names;
    std::vector<int64_t> values;
    for (const auto &b : operands) {
        if (!unique.insert(b.name).second) return failure("duplicate operand: " + b.name);
        names.push_back(b.name.c_str());
        if (const auto *n = std::get_if<int64_t>(&b.value)) values.push_back(*n);
        else {
            const auto &name = std::get<std::string>(b.value);
            auto *reg = find_register(name);
            if (!reg) return failure("unknown register: " + name);
            values.push_back(reg->enc);
        }
    }
    DsymEncoded result{};
    if (!dsym_encode_named(spec_.get(), encoder.c_str(), target, names.data(), values.data(),
                           names.size(), false, &result)) return failure(result.error);
    Encoded out;
    out.bytes.assign(result.bytes, result.bytes + result.size);
    auto *program = spec_->encoders[dsym_spec_find_encoder(spec_.get(), encoder.c_str())].program;
    for (size_t i = 0; i < result.reloc_count; ++i) {
        const auto &r = result.relocs[i];
        out.relocations.push_back({r.offset, r.size, r.reloc_index, r.reloc_name,
                                   program->params[r.sym_param], r.sym_value});
    }
    return out;
}
Result<Encoded> Spec::assemble(std::string_view source, const std::string &path,
                               const AssembleOptions &options) const {
    if (!spec_) return failure("empty spec (moved from)", path);
    if (options.target >= spec_->target_count) return failure("target index out of range", path);
    Encoded out;
    size_t line = 1;
    while (!source.empty()) {
        auto end = source.find('\n');
        auto text = source.substr(0, end);
        auto parsed = dsl::run_parser(assembly_line, text);
        if (!parsed.value) {
            auto e = parse_error(text, path, parsed.error);
            e.diagnostics[0].line = line;
            return e;
        }
        auto &insn = *parsed.value;
        if (!insn.encoder.empty()) {
            const std::vector<std::string> *params = nullptr;
            auto override = options.bindings.find(insn.encoder);
            auto defined = bindings_.find(insn.encoder);
            if (override != options.bindings.end()) params = &override->second;
            else if (defined != bindings_.end()) params = &defined->second;
            if (params) {
                std::set<std::string> unique(params->begin(), params->end());
                if (unique.size() != params->size())
                    return failure("duplicate assembly parameter", path, line, insn.offset + 1);
            }
            if (!insn.positional.empty()) {
                if (!params) return failure("positional operands require an explicit assembly binding for " + insn.encoder,
                                             path, line, insn.offset + 1);
                if (params->size() != insn.positional.size()) return failure("wrong operand count", path, line, insn.offset + 1);
                for (size_t i = 0; i < params->size(); ++i)
                    insn.named.push_back({(*params)[i], std::move(insn.positional[i])});
            }
            auto encoded = encode(insn.encoder, insn.named, options.target);
            // With an explicit source/form order we can safely apply form metadata.
            // Do not infer this mapping from bytecode parameter occurrence order.
            if (encoded && params) {
                std::vector<Operand> ordered;
                for (const auto &p : *params) {
                    auto b = std::find_if(insn.named.begin(), insn.named.end(),
                                           [&](const Binding &v) { return v.name == p; });
                    if (b == insn.named.end()) break;
                    ordered.push_back(b->value);
                }
                if (ordered.size() != params->size() || params->size() != insn.named.size())
                    return failure("assembly binding must cover all operands", path, line, insn.offset + 1);
                for (size_t i = 0; i < spec_->form_count; ++i) {
                    if (insn.encoder != spec_->forms[i].encoder) continue;
                    auto candidate = encode_form(i, *params, ordered, options.target);
                    encoded = std::move(candidate);
                    if (encoded) break;
                }
            }
            if (!encoded) {
                auto e = *encoded.error();
                for (auto &d : e.diagnostics) { d.file = path; d.line = line; d.column = insn.offset + 1; }
                return e;
            }
            auto &v = *encoded.get();
            if (out.bytes.size() > options.max_output_bytes || v.bytes.size() > options.max_output_bytes - out.bytes.size())
                return failure("assembly output byte limit exceeded", path, line, insn.offset + 1);
            for (auto &r : v.relocations) { r.offset += out.bytes.size(); out.relocations.push_back(std::move(r)); }
            out.bytes.insert(out.bytes.end(), v.bytes.begin(), v.bytes.end());
        }
        if (end == std::string_view::npos) break;
        source.remove_prefix(end + 1); ++line;
    }
    return out;
}

Result<Encoded> Spec::encode_form(size_t index, std::span<const std::string> parameters,
                                  std::span<const Operand> operands, size_t target) const {
    if (!spec_) return failure("empty spec (moved from)");
    if (index >= spec_->form_count) return failure("form index out of range");
    if (target >= spec_->target_count) return failure("target index out of range");
    const auto &form = spec_->forms[index];
    if (!form.has_operands) return failure("form has no operand metadata");
    if (parameters.size() != form.operand_count || operands.size() != form.operand_count)
        return failure("form operand count mismatch");
    const auto &features = spec_->targets[target].features;
    auto enabled = [&](const char *name) {
        for (size_t i = 0; i < features.count; ++i)
            if (std::string_view(features.items[i]) == name) return true;
        return false;
    };
    for (size_t i = 0; i < form.required.count; ++i)
        if (!enabled(form.required.items[i])) return failure("required target feature: " + std::string(form.required.items[i]));
    for (size_t i = 0; i < form.forbidden.count; ++i)
        if (enabled(form.forbidden.items[i])) return failure("forbidden target feature: " + std::string(form.forbidden.items[i]));
    std::vector<Binding> bindings;
    for (size_t i = 0; i < operands.size(); ++i) {
        const auto &desc = form.operands[i];
        auto bad = [&](const std::string &message) { return failure("operand " + std::to_string(i + 1) + ": " + message); };
        if (desc.hidden || desc.constraint) return bad("hidden operands and custom constraints are not supported");
        if (desc.kind == DSYM_OK_REG) {
            const auto *name = std::get_if<std::string>(&operands[i]);
            if (!name) return bad("expected register name");
            const auto *reg = find_register(*name);
            if (!reg) return bad("unknown register: " + *name);
            if (desc.has_reg_class && reg->reg_class != desc.reg_class) return bad("wrong register class");
            if (desc.has_width && reg->width != desc.width) return bad("wrong register width");
        } else if (desc.kind == DSYM_OK_IMM || desc.kind == DSYM_OK_DISP ||
                   desc.kind == DSYM_OK_ABS || desc.kind == DSYM_OK_REL || desc.kind == DSYM_OK_COND) {
            const auto *value = std::get_if<int64_t>(&operands[i]);
            if (!value) return bad("expected integer");
            if ((desc.has_imm_min && *value < desc.imm_min) || (desc.has_imm_max && *value > desc.imm_max))
                return bad("integer outside form bounds");
            if (desc.has_width && desc.width > 0 && desc.width < 64) {
                if (desc.has_signed && desc.is_signed) {
                    int64_t bound = int64_t(1) << (desc.width - 1);
                    if (*value < -bound || *value >= bound) return bad("integer outside signed operand width");
                } else if (desc.has_signed) {
                    if (*value < 0 || uint64_t(*value) >= (uint64_t(1) << desc.width))
                        return bad("integer outside unsigned operand width");
                }
            }
            if (desc.has_signed && !desc.is_signed && *value < 0) return bad("negative unsigned operand");
        } else return bad("unsupported operand kind");
        bindings.push_back({parameters[i], operands[i]});
    }
    return encode(form.encoder, bindings, target);
}
} // namespace metaarch
