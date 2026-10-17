#ifndef LDTK_HPP_INCLUDED
#define LDTK_HPP_INCLUDED

// LDtk.hpp - header-only ELF64 x86-64 static linker.
// Requires C++20 and DSLtk.hpp. Initial backend: ELF64, little-endian,
// x86-64 ET_REL inputs -> a simple static ET_EXEC image (no dynamic linking).
// This is intentionally not a replacement for GNU ld/lld; unsupported ELF
// features and relocation kinds are reported as errors rather than guessed.

#include "Common.hpp"
#include <bit>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ld_tk {

struct Diagnostic {
    std::string message;
    std::string object;
    std::size_t offset = 0;
};

struct Options {
    std::uint64_t image_base = 0x400000;
    std::string entry_symbol = "_start";
    std::uint64_t max_output_size = 256ull * 1024 * 1024;
    bool allow_undefined = false; // retained for compatibility; rejected for ET_EXEC
    std::uint64_t max_input_size = 256ull * 1024 * 1024;
    std::uint64_t max_total_input_size = 512ull * 1024 * 1024;
    std::size_t max_objects = 1024;
};

struct Result {
    std::vector<std::uint8_t> image;
    std::vector<Diagnostic> diagnostics;
    dsl::ASTNode report = dsl::ASTNode("link-report", std::vector<dsl::ASTNode>{});
    explicit operator bool() const noexcept { return diagnostics.empty() && !image.empty(); }
};

namespace detail {
constexpr std::uint32_t SHT_NULL=0, SHT_PROGBITS=1, SHT_SYMTAB=2,
    SHT_STRTAB=3, SHT_RELA=4, SHT_NOTE=7, SHT_NOBITS=8, SHT_REL=9;
constexpr std::uint64_t SHF_WRITE=1, SHF_ALLOC=2, SHF_EXECINSTR=4;
constexpr std::uint16_t ET_REL=1, ET_EXEC=2, EM_X86_64=62;
constexpr std::uint32_t PT_LOAD=1;
constexpr std::uint32_t PF_X=1, PF_W=2, PF_R=4;
constexpr std::uint32_t R_X86_64_64=1, R_X86_64_PC32=2, R_X86_64_PLT32=4,
    R_X86_64_32=10, R_X86_64_32S=11;
constexpr std::uint8_t ELFCLASS64=2, ELFDATA2LSB=1, EV_CURRENT=1;

#pragma pack(push, 1)
struct Ehdr {
    unsigned char ident[16]; std::uint16_t type, machine; std::uint32_t version;
    std::uint64_t entry, phoff, shoff; std::uint32_t flags;
    std::uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct Phdr { std::uint32_t type, flags; std::uint64_t offset, vaddr, paddr, filesz, memsz, align; };
struct Shdr { std::uint32_t name, type; std::uint64_t flags, addr, offset, size;
    std::uint32_t link, info; std::uint64_t addralign, entsize; };
struct Sym { std::uint32_t name; std::uint8_t info, other; std::uint16_t shndx;
    std::uint64_t value, size; };
struct Rela { std::uint64_t offset, info; std::int64_t addend; };
#pragma pack(pop)
static_assert(sizeof(Ehdr)==64 && sizeof(Phdr)==56 && sizeof(Shdr)==64 && sizeof(Sym)==24 && sizeof(Rela)==24);

inline bool range_ok(std::size_t n, std::uint64_t off, std::uint64_t len) {
    return off <= n && len <= n - static_cast<std::size_t>(off);
}
template<class T> inline bool read_at(const std::vector<std::uint8_t>& b, std::uint64_t off, T& out) {
    if (!range_ok(b.size(), off, sizeof(T))) return false;
    std::memcpy(&out, b.data()+off, sizeof(T)); return true;
}
inline std::string str_at(const std::vector<std::uint8_t>& b, std::uint64_t off, std::uint64_t size, std::uint32_t idx) {
    if (idx >= size || !range_ok(b.size(), off, size)) return {};
    const char* p = reinterpret_cast<const char*>(b.data()+off+idx);
    const std::size_t remain = static_cast<std::size_t>(size-idx);
    const void* end = std::memchr(p, 0, std::min<std::size_t>(remain,4097));
    if (!end) return {};
    return std::string(p, static_cast<const char*>(end)-p);
}
inline std::uint64_t align_up(std::uint64_t n, std::uint64_t a) {
    if (a <= 1) return n;
    const auto rem = n % a;
    if (rem && n > UINT64_MAX - (a-rem)) return UINT64_MAX;
    return rem ? n + (a-rem) : n;
}
inline bool power_two(std::uint64_t a) { return a == 0 || (a & (a-1)) == 0; }
// Exact S+A[-P] without compiler-specific 128-bit integers or signed overflow.
inline std::optional<std::uint64_t> relocated(std::uint64_t s, std::int64_t a,
        std::uint64_t p, bool relative, unsigned width, bool signed_field) {
    struct Wide { std::uint64_t lo = 0, hi = 0; } positive{s,0}, negative{};
    auto add = [](Wide& w, std::uint64_t v) { const auto old = w.lo; w.lo += v; w.hi += w.lo < old; };
    if (a < 0) add(negative, std::uint64_t{0} - static_cast<std::uint64_t>(a));
    else add(positive, static_cast<std::uint64_t>(a));
    if (relative) add(negative, p);
    const bool neg = positive.hi < negative.hi || (positive.hi == negative.hi && positive.lo < negative.lo);
    const Wide big = neg ? negative : positive, small = neg ? positive : negative;
    const Wide diff{big.lo - small.lo, big.hi - small.hi - (big.lo < small.lo)};
    if (diff.hi) return std::nullopt;
    if (signed_field) {
        const auto limit = (std::uint64_t{1} << (width*8-1));
        if (diff.lo > limit - (neg ? 0u : 1u)) return std::nullopt;
    } else if (width < 8 && (neg || diff.lo > UINT32_MAX)) return std::nullopt;
    return neg ? std::uint64_t{0} - diff.lo : diff.lo;
}
inline void put_u(std::vector<std::uint8_t>& b, std::uint64_t off, std::uint64_t v, unsigned width) {
    for (unsigned i=0; i<width; ++i) b.at(static_cast<std::size_t>(off+i)) = static_cast<std::uint8_t>(v >> (8*i));
}
struct InputSection { Shdr h{}; std::string name; std::size_t out_index=0; std::uint64_t out_offset=0; };
struct InputSymbol { Sym s{}; std::string name; };
struct InputObject {
    std::string name; std::vector<std::uint8_t> bytes; Ehdr eh{};
    std::vector<Shdr> headers; std::vector<InputSection> sections;
    std::vector<InputSymbol> symbols;
    std::uint32_t symtab_index=0, strtab_index=0;
};
struct OutputSection { std::string name; std::uint64_t flags=0, align=1, offset=0, addr=0; std::vector<std::uint8_t> data; };
struct SymbolDef { std::uint64_t address=0; bool defined=false; bool weak=false; std::string source; };

inline bool parse_object(InputObject& o, std::vector<Diagnostic>& errors) {
    auto fail=[&](std::string m, std::size_t off=0){ errors.push_back({std::move(m),o.name,off}); return false; };
    if (!read_at(o.bytes,0,o.eh)) return fail("file is shorter than an ELF64 header");
    const auto& id=o.eh.ident;
    if (id[0]!=0x7f || id[1]!='E' || id[2]!='L' || id[3]!='F') return fail("not an ELF file");
    if (id[4]!=ELFCLASS64) return fail("only ELF64 objects are supported");
    if (id[5]!=ELFDATA2LSB) return fail("only little-endian ELF objects are supported");
    if (id[6]!=EV_CURRENT || o.eh.version!=EV_CURRENT) return fail("unsupported ELF version");
    if (o.eh.type!=ET_REL) return fail("input must be an ELF relocatable object (ET_REL)");
    if (o.eh.machine!=EM_X86_64) return fail("only x86-64 (EM_X86_64) is supported");
    if (o.eh.flags || o.eh.phnum) return fail("unsupported ELF flags or program headers in ET_REL");
    if (o.eh.ehsize!=sizeof(Ehdr) || o.eh.shentsize!=sizeof(Shdr)) return fail("unexpected ELF header or section-header size");
    if (!o.eh.shnum || !range_ok(o.bytes.size(),o.eh.shoff,std::uint64_t(o.eh.shnum)*sizeof(Shdr))) return fail("invalid section-header table",o.eh.shoff);
    o.headers.resize(o.eh.shnum);
    for (std::size_t i=0;i<o.headers.size();++i)
        if (!read_at(o.bytes,o.eh.shoff+i*sizeof(Shdr),o.headers[i])) return fail("truncated section header",o.eh.shoff+i*sizeof(Shdr));
    const Shdr empty{};
    if (std::memcmp(&o.headers[0],&empty,sizeof(Shdr)) != 0) return fail("section zero must be the null section; extended indices are unsupported");
    if (o.eh.shstrndx>=o.headers.size()) return fail("invalid section-name string-table index");
    const Shdr& names=o.headers[o.eh.shstrndx];
    if (names.type!=SHT_STRTAB || !range_ok(o.bytes.size(),names.offset,names.size)) return fail("invalid section-name string table",names.offset);
    if (!names.size || o.bytes[static_cast<std::size_t>(names.offset)]!=0) return fail("section string table must start with NUL",names.offset);
    o.sections.resize(o.headers.size());
    for (std::size_t i=0;i<o.headers.size();++i) {
        auto& s=o.sections[i]; s.h=o.headers[i];
        if (!power_two(s.h.addralign)) return fail("section alignment is not a power of two",s.h.offset);
        if (s.h.flags & SHF_ALLOC) {
            if (s.h.type!=SHT_PROGBITS && s.h.type!=SHT_NOBITS && s.h.type!=SHT_NOTE) return fail("unsupported allocated section type: " + std::to_string(s.h.type),s.h.offset);
            if (s.h.flags & ~(SHF_WRITE|SHF_ALLOC|SHF_EXECINSTR)) return fail("unsupported allocated section flags (TLS, groups, merge, etc.)",s.h.offset);
            if ((s.h.flags & SHF_WRITE) && (s.h.flags & SHF_EXECINSTR)) return fail("writable executable section rejected",s.h.offset);
        }
        if (s.h.type == SHT_REL) return fail("SHT_REL relocations are not supported; use SHT_RELA",s.h.offset);
        s.name=str_at(o.bytes,names.offset,names.size,s.h.name);
        if (s.h.name && s.name.empty()) return fail("invalid section name",s.h.offset);
        if (s.h.type!=SHT_NOBITS && s.h.size && !range_ok(o.bytes.size(),s.h.offset,s.h.size)) return fail("section extends past end of file: "+s.name,s.h.offset);
        if (s.h.type==SHT_SYMTAB) {
            if (o.symtab_index) return fail("multiple symbol tables are not supported",s.h.offset);
            if (s.h.entsize!=sizeof(Sym) || s.h.size%sizeof(Sym) || s.h.link>=o.headers.size()) return fail("malformed symbol table",s.h.offset);
            o.symtab_index=static_cast<std::uint32_t>(i); o.strtab_index=s.h.link;
        }
    }
    if (!o.symtab_index || o.sections[o.strtab_index].h.type!=SHT_STRTAB) return fail("object has no usable symbol table");
    const auto& st=o.sections[o.strtab_index].h;
    const auto& sy=o.sections[o.symtab_index].h;
    if (!st.size || !range_ok(o.bytes.size(),st.offset,st.size) || o.bytes[static_cast<std::size_t>(st.offset)]!=0) return fail("symbol string table must start with NUL",st.offset);
    o.symbols.resize(static_cast<std::size_t>(sy.size/sizeof(Sym)));
    if (o.symbols.empty() || sy.info>o.symbols.size()) return fail("invalid symbol-table local/global boundary");
    for (std::size_t i=0;i<o.symbols.size();++i) {
        auto& sym=o.symbols[i];
        if (!read_at(o.bytes,sy.offset+i*sizeof(Sym),sym.s)) return fail("truncated symbol table",sy.offset+i*sizeof(Sym));
        if (i==0) { const Sym null_symbol{}; if (std::memcmp(&sym.s,&null_symbol,sizeof(Sym))!=0) return fail("symbol zero must be null"); }
        if (((sym.s.info>>4)==0) != (i<sy.info)) return fail("invalid local/global symbol order");
        sym.name=str_at(o.bytes,st.offset,st.size,sym.s.name);
        if (sym.s.name && sym.name.empty()) return fail("invalid symbol name",sy.offset+i*sizeof(Sym));
        if (sym.s.shndx!=0 && sym.s.shndx!=0xfff1 && sym.s.shndx>=o.sections.size()) return fail("symbol has invalid section index: "+sym.name);
        if ((sym.s.info >> 4) > 2 || (sym.s.info & 15) == 6 || (sym.s.info & 15) == 10)
            return fail("unsupported symbol binding or type: " + sym.name);
        if (sym.s.shndx && sym.s.shndx != 0xfff1) {
            const auto size = o.sections[sym.s.shndx].h.size;
            if (sym.s.value > size || sym.s.size > size - sym.s.value) return fail("symbol extends outside section: " + sym.name);
        }
    }
    return true;
}
} // namespace detail

class Linker {
public:
    explicit Linker(Options options = {}) : options_(std::move(options)) {}
    const Options& options() const noexcept { return options_; }
    void set_options(Options options) { options_=std::move(options); }

    Result link(const std::vector<std::vector<std::uint8_t>>& inputs,
                const std::vector<std::string>& names = {}) const {
        using namespace detail;
        Result result;
        auto error=[&](std::string m, std::string obj={}, std::size_t off=0) {
            result.diagnostics.push_back({std::move(m),std::move(obj),off});
        };
        if (inputs.empty()) { error("no input object files"); return result; }
        if (inputs.size() > options_.max_objects) { error("input object count limit exceeded"); return result; }
        std::uint64_t input_size=0;
        for (const auto& bytes:inputs) {
            if (bytes.size()>options_.max_input_size || bytes.size()>options_.max_total_input_size-std::min(input_size,options_.max_total_input_size)) { error("input size limit exceeded"); return result; }
            input_size+=bytes.size();
        }
        if constexpr (std::endian::native != std::endian::little) { error("ELF backend requires a little-endian host"); return result; }
        if (options_.allow_undefined) { error("allow_undefined is incompatible with static ET_EXEC output"); return result; }
        if (options_.image_base % 0x1000) { error("image_base must be page aligned"); return result; }
        std::vector<InputObject> objects; objects.reserve(inputs.size());
        for (std::size_t i=0;i<inputs.size();++i) {
            InputObject o; o.bytes=inputs[i]; o.name=i<names.size()?names[i]:("input"+std::to_string(i)+".o");
            if (!parse_object(o,result.diagnostics)) continue;
            objects.push_back(std::move(o));
        }
        if (!result.diagnostics.empty()) return result;

        // Merge allocated sections by name and compatible type/flags.
        std::vector<OutputSection> outsecs;
        std::map<std::string,std::size_t> secmap;
        std::uint64_t merged_size = 0;
        for (auto& o:objects) {
            for (std::size_t si=1;si<o.sections.size();++si) {
                auto& s=o.sections[si];
                if (!(s.h.flags&SHF_ALLOC) || s.h.type==SHT_NULL || s.h.type==SHT_SYMTAB || s.h.type==SHT_STRTAB || s.h.type==SHT_RELA || s.h.type==SHT_REL) continue;
                if (s.h.type!=SHT_PROGBITS && s.h.type!=SHT_NOBITS && s.h.type!=SHT_NOTE) { error("unsupported allocated section type: "+s.name,o.name,s.h.offset); continue; }
                std::string key=s.name+"#"+std::to_string(s.h.flags)+"#"+std::to_string(s.h.type);
                auto it=secmap.find(key);
                if (it==secmap.end()) {
                    const auto idx=outsecs.size(); secmap.emplace(key,idx);
                    outsecs.push_back(OutputSection{s.name,s.h.flags,std::max<std::uint64_t>(1,s.h.addralign),0,0,{}});
                    it=secmap.find(key);
                }
                auto& dst=outsecs[it->second];
                dst.align=std::max(dst.align,std::max<std::uint64_t>(1,s.h.addralign));
                const auto aligned=align_up(dst.data.size(),std::max<std::uint64_t>(1,s.h.addralign));
                if (aligned>options_.max_output_size || s.h.size>options_.max_output_size-aligned) { error("output size limit exceeded while merging section",o.name,s.h.offset); continue; }
                if (aligned>std::numeric_limits<std::size_t>::max() || s.h.size>std::numeric_limits<std::size_t>::max()-aligned) { error("section exceeds host addressable size",o.name,s.h.offset); continue; }
                const auto growth = aligned + s.h.size - dst.data.size();
                if (growth > options_.max_output_size - std::min(merged_size, options_.max_output_size)) { error("combined output section size limit exceeded",o.name); continue; }
                merged_size += growth;
                dst.data.resize(static_cast<std::size_t>(aligned),0);
                s.out_index=it->second; s.out_offset=aligned;
                if ((s.h.type==SHT_PROGBITS || s.h.type==SHT_NOTE) && s.h.size)
                    dst.data.insert(dst.data.end(),o.bytes.begin()+static_cast<std::ptrdiff_t>(s.h.offset),o.bytes.begin()+static_cast<std::ptrdiff_t>(s.h.offset+s.h.size));
                else dst.data.resize(static_cast<std::size_t>(aligned+s.h.size),0);
            }
        }
        if (!result.diagnostics.empty()) return result;

        // Lay out output sections after ELF and program headers.
        if (outsecs.size() > 65532) { error("too many output sections"); return result; }
        const std::uint64_t headers_size=sizeof(Ehdr)+(outsecs.size()+2)*sizeof(Phdr);
        std::uint64_t cursor=align_up(headers_size,0x1000);
        for (auto& s:outsecs) {
            if (options_.image_base > UINT64_MAX-cursor) { error("virtual address overflow"); return result; }
            const auto address=align_up(options_.image_base+cursor,std::max<std::uint64_t>(0x1000,s.align));
            if (address==UINT64_MAX) { error("virtual address alignment overflow"); return result; }
            cursor=address-options_.image_base; s.offset=cursor;
            if (s.data.size()>options_.max_output_size || cursor>options_.max_output_size-s.data.size()) { error("output size limit exceeded"); return result; }
            if (options_.image_base > UINT64_MAX - cursor || s.data.size() > UINT64_MAX - options_.image_base - cursor) { error("virtual address overflow"); return result; }
            s.addr=address;
            cursor+=s.data.size();
        }
        if (cursor>options_.max_output_size || cursor>std::numeric_limits<std::size_t>::max()) { error("output image is too large"); return result; }

        // Resolve symbols: local symbols are resolved from their own object;
        // global strong definitions must be unique. Weak definitions are fallback.
        std::map<std::string,SymbolDef> globals;
        std::vector<std::vector<std::optional<std::uint64_t>>> symaddr(objects.size());
        for (std::size_t oi=0;oi<objects.size();++oi) {
            auto& o=objects[oi]; symaddr[oi].resize(o.symbols.size());
            for (std::size_t si=0;si<o.symbols.size();++si) {
                const auto& sym=o.symbols[si]; const auto bind=sym.s.info>>4;
                if (sym.s.shndx==0) continue;
                std::uint64_t addr=0;
                if (sym.s.shndx==0xfff1) addr=sym.s.value; // SHN_ABS
                else {
                    const auto& insec=o.sections[sym.s.shndx];
                    if (!(insec.h.flags&SHF_ALLOC)) continue;
                    if (insec.out_index>=outsecs.size()) { error("symbol refers to an unlaid-out section: "+sym.name,o.name); continue; }
                    addr=outsecs[insec.out_index].addr+insec.out_offset+sym.s.value;
                }
                symaddr[oi][si]=addr;
                if (sym.name.empty() || bind==0) continue; // local or unnamed
                auto it=globals.find(sym.name);
                if (it==globals.end()) globals.emplace(sym.name,SymbolDef{addr,true,bind==2,o.name});
                else if (bind==2) { /* weak definitions never override an existing definition */ }
                else if (it->second.defined && !it->second.weak) error("duplicate global symbol: "+sym.name,o.name);
                else it->second=SymbolDef{addr,true,false,o.name};
            }
        }
        if (!result.diagnostics.empty()) return result;

        // Apply SHT_RELA relocations. Supported relocation formulas: S+A,
        // S+A-P for PC-relative fields. P is the address of the relocation site.
        for (std::size_t oi=0;oi<objects.size();++oi) {
            auto& o=objects[oi];
            for (std::size_t ri=1;ri<o.sections.size();++ri) {
                const auto& relsec=o.sections[ri];
                if (relsec.h.type!=SHT_RELA) continue;
                if (relsec.h.entsize!=sizeof(Rela) || relsec.h.size%sizeof(Rela) || relsec.h.info>=o.sections.size() || relsec.h.link!=o.symtab_index) {
                    error("malformed or unsupported relocation section",o.name,relsec.h.offset); continue;
                }
                auto& target=o.sections[relsec.h.info];
                if (!(target.h.flags&SHF_ALLOC)) continue;
                if (target.out_index>=outsecs.size()) { error("relocation target section was not laid out",o.name); continue; }
                auto& dst=outsecs[target.out_index];
                for (std::uint64_t p=0;p<relsec.h.size;p+=sizeof(Rela)) {
                    Rela r{}; if (!read_at(o.bytes,relsec.h.offset+p,r)) { error("truncated relocation",o.name,relsec.h.offset+p); break; }
                    const std::uint32_t symi=static_cast<std::uint32_t>(r.info>>32);
                    const std::uint32_t type=static_cast<std::uint32_t>(r.info&0xffffffffu);
                    if (type == 0) continue; // R_X86_64_NONE ignores symbol and offset
                    if (symi>=o.symbols.size()) { error("relocation references invalid symbol index",o.name,relsec.h.offset+p); continue; }
                    const auto& sym=o.symbols[symi];
                    std::uint64_t S=0;
                    if (symi==0) S=0;
                    else if (sym.s.shndx==0 || (sym.s.info >> 4) != 0) {
                        auto it=globals.find(sym.name);
                        if (it==globals.end() || !it->second.defined) {
                            if ((sym.s.info >> 4) == 2) S = 0; // unresolved weak symbols have value zero
                            else { error("undefined symbol: "+sym.name,o.name,relsec.h.offset+p); continue; }
                        } else {
                            S=it->second.address;
                        }
                    } else if (symaddr[oi][symi]) S=*symaddr[oi][symi];
                    else { error("relocation symbol has no output address: "+sym.name,o.name,relsec.h.offset+p); continue; }
                    unsigned width=0;
                    switch(type) {
                    case R_X86_64_64: width=8; break;
                    case R_X86_64_PC32: case R_X86_64_PLT32: case R_X86_64_32: case R_X86_64_32S: width=4; break;
                    default: error("unsupported x86-64 relocation type "+std::to_string(type),o.name,relsec.h.offset+p); continue;
                    }
                    if (r.offset>target.h.size || width>target.h.size-r.offset) { error("relocation writes outside input target section",o.name,relsec.h.offset+p); continue; }
                    const std::uint64_t place=target.out_offset+r.offset;
                    const std::uint64_t P=dst.addr+place;
                    const bool relative = type==R_X86_64_PC32 || type==R_X86_64_PLT32;
                    auto value = relocated(S,r.addend,P,relative,width,relative || type==R_X86_64_32S);
                    if (!value) { error("relocation overflow",o.name,relsec.h.offset+p); continue; }
                    put_u(dst.data,place,*value,width);
                }
            }
        }
        if (!result.diagnostics.empty()) return result;

        auto entry=globals.find(options_.entry_symbol);
        if (entry==globals.end() || !entry->second.defined) { error("entry symbol not found: "+options_.entry_symbol); return result; }
        bool executable_entry = false;
        for (const auto& s : outsecs) if ((s.flags & SHF_EXECINSTR) && entry->second.address >= s.addr && entry->second.address - s.addr < s.data.size()) executable_entry = true;
        if (!executable_entry) { error("entry symbol is not inside executable code"); return result; }

        // Static, non-PIE ET_EXEC: read-only headers, separately permissioned
        // PT_LOADs, and an explicitly non-executable PT_GNU_STACK.
        result.image.assign(static_cast<std::size_t>(cursor),0);
        Ehdr eh{}; eh.ident[0]=0x7f; eh.ident[1]='E'; eh.ident[2]='L'; eh.ident[3]='F';
        eh.ident[4]=ELFCLASS64; eh.ident[5]=ELFDATA2LSB; eh.ident[6]=EV_CURRENT;
        eh.type=ET_EXEC; eh.machine=EM_X86_64; eh.version=EV_CURRENT; eh.entry=entry->second.address;
        eh.phoff=sizeof(Ehdr); eh.ehsize=sizeof(Ehdr); eh.phentsize=sizeof(Phdr); eh.phnum=static_cast<std::uint16_t>(outsecs.size()+2);
        std::memcpy(result.image.data(),&eh,sizeof(eh));
        Phdr ph{}; ph.type=PT_LOAD; ph.flags=PF_R; ph.offset=0; ph.vaddr=options_.image_base; ph.paddr=options_.image_base;
        ph.filesz=headers_size; ph.memsz=headers_size; ph.align=0x1000;
        std::memcpy(result.image.data()+sizeof(Ehdr),&ph,sizeof(ph));
        std::size_t phindex = 1;
        for (const auto& s : outsecs) {
            Phdr segment{}; segment.type=PT_LOAD; segment.flags=PF_R | ((s.flags&SHF_WRITE)?PF_W:0) | ((s.flags&SHF_EXECINSTR)?PF_X:0);
            segment.offset=s.offset; segment.vaddr=segment.paddr=s.addr; segment.filesz=segment.memsz=s.data.size(); segment.align=0x1000;
            std::memcpy(result.image.data()+sizeof(Ehdr)+phindex++*sizeof(Phdr),&segment,sizeof(segment));
        }
        Phdr stack{}; stack.type=0x6474e551; stack.flags=PF_R|PF_W; stack.align=16;
        std::memcpy(result.image.data()+sizeof(Ehdr)+phindex*sizeof(Phdr),&stack,sizeof(stack));
        for (const auto& s:outsecs) if (!s.data.empty())
            std::memcpy(result.image.data()+s.offset,s.data.data(),s.data.size());

        std::vector<dsl::ASTNode> report_children;
        report_children.emplace_back("entry",options_.entry_symbol+" @ "+hex(entry->second.address));
        for (const auto& s:outsecs) {
            std::vector<dsl::ASTNode> fields;
            fields.emplace_back("address",hex(s.addr));
            fields.emplace_back("offset",std::to_string(s.offset));
            fields.emplace_back("size",std::to_string(s.data.size()));
            report_children.emplace_back("section",std::vector<dsl::ASTNode>{dsl::ASTNode("name",s.name),dsl::ASTNode("layout",std::move(fields))});
        }
        report_children.emplace_back("image-size",std::to_string(result.image.size()));
        result.report=dsl::ASTNode("link-report",std::move(report_children));
        return result;
    }

    Result link_files(const std::vector<std::string>& paths) const {
        Result bad;
        if (paths.size()>options_.max_objects) { bad.diagnostics.push_back({"input object count limit exceeded",{},0}); return bad; }
        std::vector<std::vector<std::uint8_t>> inputs; inputs.reserve(paths.size());
        std::uint64_t total=0;
        for (const auto& path:paths) {
            // File I/O is intentionally kept out of this header's core API.
            // Use read_file() below, or provide bytes directly to link().
            auto bytes=read_file(path,std::min(options_.max_input_size,options_.max_total_input_size-std::min(total,options_.max_total_input_size)));
            if (!bytes) { bad.diagnostics.push_back({"cannot read input file",path,0}); return bad; }
            inputs.push_back(std::move(*bytes));
            total+=inputs.back().size();
        }
        return link(inputs,paths);
    }

    static std::optional<std::vector<std::uint8_t>> read_file(const std::string& path, std::uint64_t max_bytes = 256ull*1024*1024) {
        // Implemented using standard C++ streams in the out-of-class helper below.
        return read_file_impl(path,max_bytes);
    }
    static bool write_file(const std::string& path, const std::vector<std::uint8_t>& bytes) {
        return write_file_impl(path,bytes);
    }
private:
    Options options_;
    static std::string hex(std::uint64_t value) {
        std::ostringstream out; out << "0x" << std::hex << value; return out.str();
    }
    static std::optional<std::vector<std::uint8_t>> read_file_impl(const std::string& path, std::uint64_t max_bytes);
    static bool write_file_impl(const std::string& path, const std::vector<std::uint8_t>& bytes);
};

} // namespace ld_tk

#include <fstream>
namespace ld_tk {
inline std::optional<std::vector<std::uint8_t>> Linker::read_file_impl(const std::string& path, std::uint64_t max_bytes) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f) return std::nullopt;
    const auto end=f.tellg(); if(end<0) return std::nullopt;
    if (static_cast<std::uint64_t>(end)>max_bytes || static_cast<std::uint64_t>(end)>static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()) || static_cast<std::uint64_t>(end)>std::numeric_limits<std::size_t>::max()) return std::nullopt;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    f.seekg(0); if(!bytes.empty() && !f.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))) return std::nullopt;
    return bytes;
}
inline bool Linker::write_file_impl(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    if (bytes.size()>static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) return false;
    std::ofstream f(path,std::ios::binary|std::ios::trunc);
    if(!f) return false;
    if(!bytes.empty()) f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    f.close();
    return static_cast<bool>(f);
}
} // namespace ld_tk

#endif // LDTK_HPP_INCLUDED
