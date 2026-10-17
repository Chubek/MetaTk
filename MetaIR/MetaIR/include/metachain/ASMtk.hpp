#pragma once

// Bounded two-pass x86-64 assembler (Intel destination-first syntax), raw
// output and interoperable ELF64 ET_REL output. See manual/06-assembler.md.
#include "Common.hpp"
#include "LDtk.hpp"
#include <filesystem>
#include <set>
#include <span>
#include <Arch/stdspec/asmtk/AMD64.hpp>

#if defined(METACHAIN_ENABLE_SYMSPEC)
// Arch/ sits at the repository root, which every MetaChain target has on its include path.
#if __has_include(<Arch/Symspec.hpp>)
#include <Arch/AssemblerSpec.hpp>
#else
#error "METACHAIN_ENABLE_SYMSPEC needs Arch/Symspec.hpp on the include path"
#endif
#endif

namespace asmtk {
using Byte = std::uint8_t;
struct SourceLocation { std::size_t line = 1, column = 1, offset = 0; };
struct Diagnostic { std::string message; SourceLocation location{}; };
struct Symbol { std::string name; std::size_t offset = 0; };
struct Relocation {
    std::size_t offset = 0;
    std::string symbol;
    std::uint32_t type = 0;
    std::int64_t addend = 0;
    SourceLocation location{};
};
struct AssemblyResult {
    std::vector<Byte> bytes;
    std::vector<Symbol> symbols;
    std::vector<Diagnostic> diagnostics;
    std::vector<Relocation> relocations;
    bool success() const noexcept { return diagnostics.empty(); }
    explicit operator bool() const noexcept { return success(); }
};
struct Options {
    bool little_endian_data = true;
    bool allow_unknown_directives = false; // explicit permissive mode, never default
    std::size_t max_output_bytes = 64u * 1024u * 1024u;
    std::size_t max_source_bytes = 64u * 1024u * 1024u;
    std::uint64_t origin = 0;
};
class Architecture {
#if defined(METACHAIN_ENABLE_SYMSPEC)
    std::shared_ptr<const symspec::Spec> spec_;
    std::shared_ptr<const symspec::encoding::Tables> tables_;
    std::string error_;
#endif
public:
    Architecture() = default;
    bool valid() const noexcept {
#if defined(METACHAIN_ENABLE_SYMSPEC)
        return !spec_ || tables_ != nullptr;
#else
        return true;
#endif
    }
    std::string_view error() const noexcept {
#if defined(METACHAIN_ENABLE_SYMSPEC)
        return error_;
#else
        return {};
#endif
    }
    std::string_view name() const noexcept {
#if defined(METACHAIN_ENABLE_SYMSPEC)
        if (spec_) return spec_->architecture();
#endif
        return "x86_64";
    }
#if defined(METACHAIN_ENABLE_SYMSPEC)
    explicit Architecture(symspec::Spec spec) : spec_(std::make_shared<const symspec::Spec>(std::move(spec))) {
        auto loaded = symspec::encoding::assembler_tables(*spec_);
        if (loaded) tables_ = std::make_shared<const symspec::encoding::Tables>(std::move(*loaded));
        else error_ = loaded.error().message;
    }
    const symspec::Spec* get() const noexcept { return spec_.get(); }
    std::vector<const symspec::Json*> forms(std::string_view mnemonic) const {
        return spec_ ? spec_->assembler_forms(mnemonic) : std::vector<const symspec::Json*>{};
    }
    const symspec::Json* register_info(std::string_view n) const noexcept { return spec_ ? spec_->assembler_register(n) : nullptr; }
    std::optional<symspec::RecordView> record(std::string_view n) const noexcept { return spec_ ? spec_->find_record(n) : std::nullopt; }
#endif
    std::span<const symspec::encoding::Register> registers() const noexcept {
#if defined(METACHAIN_ENABLE_SYMSPEC)
        if (spec_) return tables_ ? std::span<const symspec::encoding::Register>(tables_->registers) : std::span<const symspec::encoding::Register>{};
#endif
        return symspec::encoding::amd64::registers;
    }
    std::span<const symspec::encoding::Encoding> encodings() const noexcept {
#if defined(METACHAIN_ENABLE_SYMSPEC)
        if (spec_) return tables_ ? std::span<const symspec::encoding::Encoding>(tables_->encodings) : std::span<const symspec::encoding::Encoding>{};
#endif
        return symspec::encoding::amd64::encodings;
    }
};
#if defined(METACHAIN_ENABLE_SYMSPEC)
using ArchitectureSpec = symspec::Spec;
#endif
namespace detail {
struct Register { unsigned code = 0, width = 64; };
inline std::optional<Register> reg(const Architecture& architecture, std::string_view s) {
    const auto n = metachain::text::lower(s);
    for (const auto& r : architecture.registers())
        if (n == r.name) return Register{r.code, r.width};
    return std::nullopt;
}
struct SymbolExpression { std::string name; std::int64_t addend = 0; };
inline std::optional<SymbolExpression> symbol_expression(std::string_view s) {
    s = metachain::text::trim(s);
    const auto split = s.find_first_of("+-", 1);
    const auto n = metachain::text::trim(s.substr(0, split));
    if (!metachain::text::identifier(n)) return std::nullopt;
    SymbolExpression r{std::string(n), 0};
    if (split != s.npos) {
        auto tail = metachain::text::trim(s.substr(split + 1));
        auto v = metachain::text::signed_integer(std::string(1, s[split]) + std::string(tail));
        if (!v) return std::nullopt;
        r.addend = *v;
    }
    return r;
}
}
class Assembler {
    Architecture architecture_;
    Options options_;
    struct State {
        AssemblyResult result;
        std::set<std::string> globals, externals;
        std::map<std::string,std::size_t> definitions;
        std::size_t alignment = 1;
    };
    static void error(State& s, std::string message, SourceLocation loc) { s.result.diagnostics.push_back({std::move(message), loc}); }
    bool reserve(State& s, std::size_t n, SourceLocation loc) const {
        if (s.result.bytes.size() > options_.max_output_bytes || n > options_.max_output_bytes - s.result.bytes.size()) {
            error(s, "output size limit exceeded", loc); return false;
        }
        return true;
    }
    static void put(State& s, std::uint64_t v, unsigned width, bool little = true) {
        for (unsigned i = 0; i < width; ++i) s.result.bytes.push_back(static_cast<Byte>(v >> ((little ? i : width - 1 - i) * 8)));
    }
    static void rex(State& s, bool w, unsigned r = 0, unsigned b = 0, bool force = false) {
        const unsigned v = 0x40 | (w ? 8u : 0u) | ((r >> 3) << 2) | (b >> 3);
        if (v != 0x40 || force) put(s, v, 1);
    }
    static void modrm(State& s, unsigned r, unsigned b) { put(s, 0xc0 | ((r & 7) << 3) | (b & 7), 1); }
    bool symbolic(State& s, std::string_view operand, unsigned width, std::uint32_t type, std::int64_t bias, SourceLocation loc) const {
        auto expression = detail::symbol_expression(operand);
        if (!expression || (bias < 0 && expression->addend < INT64_MIN - bias)) {
            error(s, "invalid symbol expression", loc); return false;
        }
        s.result.relocations.push_back({s.result.bytes.size(), expression->name, type, expression->addend + bias, loc});
        put(s, 0, width); return true;
    }
    bool memory(State& s, unsigned field, std::string_view operand, SourceLocation loc) const {
        operand = metachain::text::trim(operand);
        if (operand.size() < 2 || operand.front() != '[' || operand.back() != ']') { error(s, "malformed memory operand", loc); return false; }
        auto inner = metachain::text::trim(operand.substr(1, operand.size() - 2));
        if (inner.starts_with("rel ")) {
            put(s, ((field & 7) << 3) | 5, 1);
            return symbolic(s, inner.substr(4), 4, 2, -4, loc);
        }
        const auto split = inner.find_first_of("+-", 1);
        auto base = detail::reg(architecture_, metachain::text::trim(inner.substr(0, split)));
        if (!base || base->width != 64) { error(s, "expected [base+disp] or [rel symbol]", loc); return false; }
        std::int64_t displacement = 0;
        if (split != inner.npos) {
            auto v = metachain::text::signed_integer(std::string(1, inner[split]) + std::string(metachain::text::trim(inner.substr(split + 1))));
            if (!v || *v < INT32_MIN || *v > INT32_MAX) { error(s, "memory displacement exceeds signed 32 bits", loc); return false; }
            displacement = *v;
        }
        // Always disp32: deterministic size, no instruction relaxation.
        put(s, 0x80 | ((field & 7) << 3) | (base->code & 7), 1);
        if ((base->code & 7) == 4) put(s, 0x24, 1); // SIB: no index
        put(s, static_cast<std::uint64_t>(displacement), 4); return true;
    }
    unsigned memory_base(std::string_view operand) const {
        if (operand.size() < 2) return 0;
        auto inner = metachain::text::trim(operand.substr(1, operand.size()-2));
        auto r = detail::reg(architecture_, metachain::text::trim(inner.substr(0, inner.find_first_of("+-", 1))));
        return r ? r->code : 0;
    }
    void instruction(State& s, std::string_view head, const std::vector<std::string_view>& a, SourceLocation loc) const {
        using symspec::encoding::Form;
        const auto lhs = a.empty() ? std::nullopt : detail::reg(architecture_, a[0]);
        const auto rhs = a.size() < 2 ? std::nullopt : detail::reg(architecture_, a[1]);
        const auto is_memory = [](std::string_view operand) { return !operand.empty() && operand.front() == '['; };
        bool known = false;
        for (const auto& e : architecture_.encodings()) {
            if (e.mnemonic != head) continue;
            known = true;
            const auto accepts = [&](const auto& r) { return r && (e.widths & symspec::encoding::width_mask(r->width)); };
            const auto opcode = [&](unsigned add = 0) {
                for (unsigned i = 0; i < e.opcode_size; ++i)
                    put(s, e.opcode[i] + (i + 1 == e.opcode_size ? add : 0), 1);
            };
            switch (e.form) {
            case Form::Fixed:
                if (!a.empty()) continue;
                opcode(); return;
            case Form::Relative:
                if (a.size() != 1 || lhs || !detail::symbol_expression(a[0])) continue;
                opcode(); symbolic(s, a[0], 4, e.relocation, -4, loc); return;
            case Form::Indirect: case Form::OpcodeRegister: case Form::Unary:
                if (a.size() != 1 || !accepts(lhs)) continue;
                rex(s, e.rex_w && lhs->width == 64, 0, lhs->code);
                opcode(e.form == Form::OpcodeRegister ? lhs->code & 7 : 0);
                if (e.form != Form::OpcodeRegister) modrm(s, e.group, lhs->code);
                return;
            case Form::RegisterRegister: {
                if (a.size() != 2 || !accepts(lhs) || !rhs || lhs->width != rhs->width) continue;
                const auto r = e.reg_operand == 0 ? *lhs : *rhs;
                const auto b = e.reg_operand == 0 ? *rhs : *lhs;
                rex(s, e.rex_w && lhs->width == 64, r.code, b.code,
                    lhs->width == 8 && (r.code >= 4 || b.code >= 4));
                opcode(); modrm(s, r.code, b.code); return;
            }
            case Form::Load: case Form::Store: {
                if (a.size() != 2) continue;
                const auto r = e.form == Form::Load ? lhs : rhs;
                const auto operand = a[e.form == Form::Load ? 1 : 0];
                if (!accepts(r) || !is_memory(operand)) continue;
                rex(s, e.rex_w && r->width == 64, r->code, memory_base(operand), r->width == 8 && r->code >= 4);
                opcode(); memory(s, r->code, operand, loc); return;
            }
            case Form::MoveImmediate: {
                if (a.size() != 2 || !accepts(lhs) || rhs || is_memory(a[1])) continue;
                const unsigned width = lhs->width / 8;
                auto v = metachain::text::integer(a[1]);
                rex(s, e.rex_w && lhs->width == 64, 0, lhs->code, lhs->width == 8 && lhs->code >= 4);
                opcode(lhs->code & 7);
                if (v) {
                    if (!v->fits(width)) error(s, "immediate does not fit register", loc);
                    else put(s, v->bits(), width);
                } else if (width == 1) error(s, "byte immediate must be an integer", loc);
                else symbolic(s, a[1], width, width == 8 ? 1 : 10, 0, loc);
                return;
            }
            case Form::ShiftCL: {
                const auto count = detail::reg(architecture_, e.implicit_register);
                if (a.size() != 2 || !accepts(lhs) || !rhs || !count || rhs->width != count->width || rhs->code != count->code) continue;
                rex(s, e.rex_w && lhs->width == 64, 0, lhs->code);
                opcode(); modrm(s, e.group, lhs->code); return;
            }
            case Form::ShiftImmediate: {
                if (a.size() != 2 || !accepts(lhs) || rhs) continue;
                auto v = metachain::text::integer(a[1]);
                if (!v || v->negative || v->magnitude >= lhs->width) continue;
                rex(s, e.rex_w && lhs->width == 64, 0, lhs->code);
                opcode(); modrm(s, e.group, lhs->code); put(s, v->magnitude, 1); return;
            }
            case Form::ArithmeticImmediate: {
                if (a.size() != 2 || !accepts(lhs) || rhs) continue;
                auto v = metachain::text::signed_integer(a[1]);
                if (!v || *v < INT32_MIN || *v > INT32_MAX) continue;
                rex(s, e.rex_w && lhs->width == 64, 0, lhs->code);
                opcode(); modrm(s, e.group, lhs->code); put(s, static_cast<std::uint64_t>(*v), 4); return;
            }
            }
        }
        error(s, (known ? "unsupported operands for instruction: " : "unknown instruction: ") + std::string(head), loc);
    }
    void statement(State& s, std::string_view line, SourceLocation loc) const {
        const auto split = line.find_first_of(" \t");
        const auto head = metachain::text::lower(line.substr(0, split));
        const auto tail = split == line.npos ? std::string_view{} : metachain::text::trim(line.substr(split));
        const auto args = metachain::text::operands(tail);
        if (head == ".text") { if (!args.empty()) error(s, ".text takes no operands", loc); return; }
        if (head == ".global" || head == ".globl" || head == ".extern") {
            if (args.empty()) error(s, "symbol declaration requires names", loc);
            for (auto a : args) {
                if (!metachain::text::identifier(a)) error(s, "invalid declared symbol", loc);
                else (head == ".extern" ? s.externals : s.globals).insert(std::string(a));
            }
            return;
        }
        unsigned width = 0;
        if (head == ".byte" || head == "db") width = 1;
        if (head == ".word" || head == ".short" || head == "dw") width = 2;
        if (head == ".long" || head == ".int" || head == "dd") width = 4;
        if (head == ".quad" || head == ".8byte" || head == "dq") width = 8;
        if (width) {
            if (args.empty()) error(s, "data directive requires operands", loc);
            for (auto a : args) {
                if (!reserve(s, width, loc)) return;
                auto v = metachain::text::integer(a);
                if (v && v->fits(width)) put(s, v->bits(), width, options_.little_endian_data);
                else if (!v && (width == 4 || width == 8) && options_.little_endian_data) symbolic(s, a, width, width == 8 ? 1 : 10, 0, loc);
                else error(s, "integer does not fit data field", loc);
            }
            return;
        }
        if (head == ".ascii" || head == ".asciz" || head == ".string") {
            if (args.empty()) error(s, "string directive requires operands", loc);
            for (auto a : args) {
                auto v = metachain::text::unquote(a);
                if (!v) { error(s, "invalid quoted string", loc); continue; }
                const auto extra = head == ".ascii" ? 0u : 1u;
                if (!reserve(s, v->size() + extra, loc)) return;
                s.result.bytes.insert(s.result.bytes.end(), v->begin(), v->end());
                if (extra) put(s, 0, 1);
            }
            return;
        }
        if (head == ".zero" || head == ".space" || head == ".skip" || head == ".align" || head == ".balign") {
            auto v = args.size() == 1 ? metachain::text::integer(args[0]) : std::nullopt;
            if (!v || v->negative || v->magnitude > options_.max_output_bytes) { error(s, "invalid size/alignment", loc); return; }
            std::uint64_t n = v->magnitude;
            if (head == ".align" || head == ".balign") {
                if (!n || !ld_tk::detail::power_two(n)) { error(s, "alignment must be a positive power of two", loc); return; }
                s.alignment = std::max(s.alignment, static_cast<std::size_t>(n));
                n = ld_tk::detail::align_up(s.result.bytes.size(), n) - s.result.bytes.size();
            }
            if (reserve(s, static_cast<std::size_t>(n), loc)) s.result.bytes.resize(s.result.bytes.size() + static_cast<std::size_t>(n), 0);
            return;
        }
        if (!head.empty() && head[0] == '.') {
            if (!options_.allow_unknown_directives) error(s, "unsupported directive: " + head, loc);
            return;
        }
        // Encoding descriptors bound every instruction to a small scratch
        // allowance. Check actual size and roll back transactionally.
        const auto old = s.result.bytes.size(), nr = s.result.relocations.size(), ne = s.result.diagnostics.size();
        instruction(s, head, args, loc);
        if (s.result.bytes.size() > options_.max_output_bytes) error(s, "output size limit exceeded", loc);
        if (s.result.diagnostics.size() != ne) { s.result.bytes.resize(old); s.result.relocations.resize(nr); }
    }
    void object(State& s) const {
        using namespace ld_tk::detail;
        if constexpr (std::endian::native != std::endian::little) { error(s, "ELF backend requires little-endian host", {}); return; }
        std::vector<Byte> strings{0};
        auto str = [&](const std::string& n) { const auto p = strings.size(); strings.insert(strings.end(), n.begin(), n.end()); strings.push_back(0); return static_cast<std::uint32_t>(p); };
        std::map<std::string, std::size_t> defined;
        for (const auto& sym : s.result.symbols) defined.emplace(sym.name, sym.offset);
        for (const auto& g : s.globals) if (!defined.contains(g)) { error(s, "global symbol is not defined: " + g, {}); return; }
        std::vector<Sym> symbols(1);
        std::map<std::string, std::uint32_t> indices;
        auto add = [&](const std::string& n, bool global, bool def, std::size_t offset) {
            Sym sym{}; sym.name = str(n); sym.info = global ? 0x10 : 0; sym.shndx = def ? 1 : 0; sym.value = offset;
            indices.emplace(n, static_cast<std::uint32_t>(symbols.size())); symbols.push_back(sym);
        };
        for (const auto& [n,p] : defined) if (!s.globals.contains(n)) add(n,false,true,p);
        const auto first_global = symbols.size();
        for (const auto& [n,p] : defined) if (s.globals.contains(n)) add(n,true,true,p);
        for (const auto& rel : s.result.relocations) if (!indices.contains(rel.symbol)) {
            if (!s.externals.contains(rel.symbol)) { error(s, "undefined symbol requires .extern: " + rel.symbol, rel.location); continue; }
            add(rel.symbol,true,false,0);
        }
        if (!s.result.diagnostics.empty()) return;
        std::vector<Rela> relocations;
        for (const auto& r : s.result.relocations) relocations.push_back({r.offset, (std::uint64_t(indices.at(r.symbol)) << 32) | r.type, r.addend});
        static constexpr char names[] = "\0.text\0.rela.text\0.symtab\0.strtab\0.shstrtab\0.note.GNU-stack\0";
        std::vector<Byte> output(sizeof(Ehdr), 0);
        std::vector<Shdr> sh(7);
        auto section = [&](unsigned idx, std::uint32_t n, std::uint32_t type, std::uint64_t flags, std::uint64_t align, const void* data, std::size_t size) {
            output.resize(static_cast<std::size_t>(align_up(output.size(), align)), 0);
            sh[idx].name=n; sh[idx].type=type; sh[idx].flags=flags; sh[idx].addralign=align; sh[idx].offset=output.size(); sh[idx].size=size;
            if (size) { auto p = static_cast<const Byte*>(data); output.insert(output.end(), p, p+size); }
        };
        section(1,1,SHT_PROGBITS,SHF_ALLOC|SHF_EXECINSTR,s.alignment,s.result.bytes.data(),s.result.bytes.size());
        section(2,7,SHT_RELA,0,8,relocations.data(),relocations.size()*sizeof(Rela)); sh[2].link=3; sh[2].info=1; sh[2].entsize=sizeof(Rela);
        section(3,18,SHT_SYMTAB,0,8,symbols.data(),symbols.size()*sizeof(Sym)); sh[3].link=4; sh[3].info=static_cast<std::uint32_t>(first_global); sh[3].entsize=sizeof(Sym);
        section(4,26,SHT_STRTAB,0,1,strings.data(),strings.size());
        section(5,34,SHT_STRTAB,0,1,names,sizeof(names));
        section(6,44,SHT_PROGBITS,0,1,nullptr,0);
        output.resize(static_cast<std::size_t>(align_up(output.size(),8)),0);
        Ehdr eh{}; eh.ident[0]=0x7f; eh.ident[1]='E'; eh.ident[2]='L'; eh.ident[3]='F'; eh.ident[4]=2; eh.ident[5]=1; eh.ident[6]=1;
        eh.type=ET_REL; eh.machine=EM_X86_64; eh.version=1; eh.ehsize=sizeof(Ehdr); eh.shoff=output.size(); eh.shentsize=sizeof(Shdr); eh.shnum=7; eh.shstrndx=5;
        const auto* table = reinterpret_cast<const Byte*>(sh.data()); output.insert(output.end(),table,table+sh.size()*sizeof(Shdr));
        if (output.size() > options_.max_output_bytes) { error(s,"ELF object exceeds output size limit",{}); return; }
        std::memcpy(output.data(),&eh,sizeof(eh)); s.result.bytes=std::move(output);
    }
    AssemblyResult assemble_impl(std::string_view source, bool elf) const {
        State s;
        if (!architecture_.valid()) { error(s, std::string(architecture_.error()), {}); return s.result; }
        if (source.size() > options_.max_source_bytes) { error(s,"source size limit exceeded",{}); return s.result; }
        std::size_t pos=0, line_no=1;
        while (pos <= source.size()) {
            auto end=source.find('\n',pos); if (end==source.npos) end=source.size();
            auto line=metachain::text::trim(metachain::text::uncomment(source.substr(pos,end-pos)));
            SourceLocation loc{line_no,source.empty() ? 1u : static_cast<std::size_t>(line.data()-source.data()-pos+1),pos};
            while (!line.empty()) {
                const auto colon=line.find(':'); if (colon==line.npos) break;
                auto n=metachain::text::trim(line.substr(0,colon)); if (!metachain::text::identifier(n)) break;
                if (n.size()>4096) error(s,"symbol name exceeds 4096 bytes",loc);
                else if (!s.definitions.emplace(std::string(n),s.result.bytes.size()).second) error(s,"duplicate label: "+std::string(n),loc);
                else s.result.symbols.push_back({std::string(n),s.result.bytes.size()});
                line=metachain::text::trim(line.substr(colon+1));
            }
            if (!line.empty()) statement(s,line,loc);
            if (end==source.size()) break;
            pos=end+1; ++line_no;
        }
        if (s.result.diagnostics.empty()) {
            if (elf) object(s);
            else for (const auto& rel:s.result.relocations) {
                auto it=s.definitions.find(rel.symbol);
                if (it==s.definitions.end()) { error(s,"undefined symbol: "+rel.symbol,rel.location); continue; }
                if (options_.origin > UINT64_MAX-it->second || options_.origin > UINT64_MAX-rel.offset) { error(s,"origin address overflow",rel.location); continue; }
                const bool pc=rel.type==2 || rel.type==4;
                auto v=ld_tk::detail::relocated(options_.origin+it->second,rel.addend,options_.origin+rel.offset,pc,rel.type==1?8:4,pc);
                if (!v) { error(s,"relocation overflow",rel.location); continue; }
                ld_tk::detail::put_u(s.result.bytes,rel.offset,*v,rel.type==1?8:4);
            }
        }
        // Never expose partially encoded machine code as a usable artifact.
        if (!s.result.diagnostics.empty()) s.result.bytes.clear();
        return std::move(s.result);
    }
public:
    explicit Assembler(Options o = {}) : options_(o) {}
    explicit Assembler(Architecture a, Options o = {}) : architecture_(std::move(a)), options_(o) {}
    const Architecture& architecture() const noexcept { return architecture_; }
    void set_architecture(Architecture a) { architecture_=std::move(a); }
    const Options& options() const noexcept { return options_; }
    void set_options(Options o) noexcept { options_=o; }
#if defined(METACHAIN_ENABLE_SYMSPEC)
    symspec::Status load_architecture(const std::filesystem::path& p, const symspec::LoadOptions& o = {}) {
        auto loaded=symspec::Spec::load(p,o); if (!loaded) return symspec::Unexpected{loaded.error()};
        Architecture candidate(std::move(*loaded));
        if (!candidate.valid()) return symspec::fail(symspec::ErrorCode::schema, std::string(candidate.error()), p);
        architecture_=std::move(candidate); return std::monostate{};
    }
#endif
    AssemblyResult assemble(std::string_view source) const { return assemble_impl(source,false); }
    AssemblyResult assemble_object(std::string_view source) const { return assemble_impl(source,true); }
};
} // namespace asmtk
