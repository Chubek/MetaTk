#include "Internal.hpp"
#include <algorithm>
#include <locale>
#include <sstream>

namespace metaas::detail {
std::string hex(const std::vector<std::uint8_t>& bytes) {
    static const char digits[]="0123456789abcdef";
    if (bytes.size()>std::string().max_size()/2) throw std::length_error("hex output size overflow");
    std::string out; out.reserve(bytes.size()*2);
    for (auto b:bytes) { out.push_back(digits[b>>4]); out.push_back(digits[b&15]); }
    return out;
}
}
namespace metaas {
namespace {
std::string quote(std::string_view text) {
    const char digits[]="0123456789abcdef";
    std::string out="\"";
    for (std::size_t i=0;i<text.size();++i) {
        const auto c=static_cast<unsigned char>(text[i]);
        if (c=='"' || c=='\\') { out+='\\'; out+=static_cast<char>(c); }
        else if (c<32) { out+="\\u00"; out+=digits[c>>4]; out+=digits[c&15]; }
        else if (c>=128) {
            const std::size_t count=c>=0xc2 && c<=0xdf ? 2 : c>=0xe0 && c<=0xef ? 3 : c>=0xf0 && c<=0xf4 ? 4 : 0;
            if (!count || count>text.size()-i) throw std::invalid_argument("invalid UTF-8 in object metadata");
            std::uint32_t codepoint=c&((1u<<(7-count))-1);
            for (std::size_t n=1;n<count;++n) {
                const auto next=static_cast<unsigned char>(text[i+n]);
                if ((next&0xc0)!=0x80) throw std::invalid_argument("invalid UTF-8 in object metadata");
                codepoint=(codepoint<<6)|(next&0x3f);
            }
            if ((count==3 && codepoint<0x800) || (count==4 && codepoint<0x10000) ||
                (codepoint>=0xd800 && codepoint<=0xdfff) || codepoint>0x10ffff)
                throw std::invalid_argument("invalid UTF-8 in object metadata");
            out.append(text.substr(i,count)); i+=count-1;
        }
        else out+=static_cast<char>(c);
    }
    return out+'"';
}
}
Result<std::vector<std::uint8_t>> Image::binary(std::size_t max_bytes) const {
    try {
        std::uint64_t end=origin;
        for (const auto& sec:sections) {
            if (!sec.alignment || (sec.alignment&(sec.alignment-1)) || sec.address%sec.alignment)
                throw std::invalid_argument("invalid section alignment");
            if (sec.address<end || sec.address-origin>max_bytes || sec.bytes.size()>max_bytes-(sec.address-origin))
                throw std::invalid_argument("section overlap or binary output limit exceeded");
            if (sec.bytes.size()>UINT64_MAX-sec.address) throw std::invalid_argument("section address overflow");
            end=sec.address+sec.bytes.size();
        }
        std::vector<std::uint8_t> result(static_cast<std::size_t>(end-origin),0);
        for (const auto& sec:sections)
            std::copy(sec.bytes.begin(),sec.bytes.end(),result.begin()+static_cast<std::ptrdiff_t>(sec.address-origin));
        std::map<std::string,const Symbol*,std::less<>> by_name;
        for (const auto& symbol:symbols) if (!by_name.emplace(symbol.name,&symbol).second) throw std::invalid_argument("duplicate image symbol");
        for (const auto& r:relocations) {
            if (!r.data) throw std::invalid_argument("architecture relocation " + r.type + " requires a linker; emit --format json");
            if (r.section>=sections.size()) throw std::invalid_argument("relocation section is invalid");
            const auto& section=sections[r.section];
            if (r.offset>section.bytes.size() || r.size>section.bytes.size()-r.offset ||
                r.bit_offset!=0 || r.bit_width!=8*r.size || r.pc_relative)
                throw std::invalid_argument("invalid data relocation field");
            auto found=by_name.find(r.symbol);
            if (found==by_name.end() || !found->second->section) throw std::invalid_argument("unresolved external symbol: " + r.symbol);
            const auto& symbol=*found->second;
            if (*symbol.section>=sections.size() || symbol.offset>sections[*symbol.section].bytes.size()) throw std::invalid_argument("invalid symbol section or offset");
            auto base=sections[*symbol.section].address;
            if (base>INT64_MAX || symbol.offset>INT64_MAX-base) throw std::invalid_argument("symbol address overflow");
            auto value=detail::add(static_cast<std::int64_t>(base+symbol.offset),r.addend);
            detail::store(result,static_cast<std::size_t>(section.address-origin)+r.offset,r.size,value,r.big_endian);
        }
        return {std::move(result),{}};
    } catch (const std::exception& e) { return {std::nullopt,{{"",e.what(),0,0}}}; }
}
std::string Image::json() const {
    std::ostringstream out; out.imbue(std::locale::classic());
    out << "{\n  \"format\": \"MetaAS\", \"version\": 1,\n  \"architecture\": " << quote(architecture)
        << ", \"subarchitecture\": " << quote(subarchitecture) << ",\n  \"object_format\": " << quote(object_format)
        << ", \"abi\": " << quote(abi) << ", \"target\": " << target << ",\n  \"address_width\": " << address_width
        << ", \"byte_order\": " << quote(big_endian?"big":"little") << ", \"origin\": " << origin << ",\n  \"sections\": [";
    for (std::size_t n=0;n<sections.size();++n) {
        const auto& s=sections[n];
        out << (n?",\n":"\n") << "    {\"name\": " << quote(s.name) << ", \"address\": " << s.address
            << ", \"alignment\": " << s.alignment << ", \"bytes\": " << quote(detail::hex(s.bytes)) << '}';
    }
    out << "\n  ],\n  \"symbols\": [";
    for (std::size_t n=0;n<symbols.size();++n) {
        const auto& s=symbols[n];
        out << (n?",\n":"\n") << "    {\"name\": " << quote(s.name) << ", \"section\": ";
        if (s.section) out << *s.section; else out << "null";
        out << ", \"offset\": " << s.offset << ", \"global\": " << (s.global?"true":"false") << '}';
    }
    out << "\n  ],\n  \"relocations\": [";
    for (std::size_t n=0;n<relocations.size();++n) {
        const auto& r=relocations[n];
        out << (n?",\n":"\n") << "    {\"section\": " << r.section << ", \"offset\": " << r.offset
            << ", \"size\": " << r.size << ", \"type\": " << quote(r.type) << ", \"symbol\": " << quote(r.symbol)
            << ", \"parameter\": " << quote(r.parameter) << ", \"addend\": " << r.addend
            << ", \"bit_offset\": " << r.bit_offset << ", \"bit_width\": " << r.bit_width
            << ", \"byte_order\": " << quote(r.big_endian?"big":"little")
            << ", \"pc_relative\": " << (r.pc_relative?"true":"false") << ", \"signed\": " << (r.is_signed?"true":"false")
            << ", \"expression\": " << quote(r.expression) << ", \"addend_mode\": " << quote(r.addend_mode)
            << ", \"overflow\": " << quote(r.overflow) << ", \"data\": " << (r.data?"true":"false") << '}';
    }
    out << "\n  ]\n}\n";
    return out.str();
}
} // namespace metaas
