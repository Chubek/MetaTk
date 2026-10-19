#pragma once
#include "MetaAS.hpp"
#include <bit>
#include <limits>
#include <stdexcept>

namespace metaas::detail {
inline std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front()==' ' || s.front()=='\t' || s.front()=='\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back()==' ' || s.back()=='\t' || s.back()=='\r')) s.remove_suffix(1);
    return s;
}
inline bool first(char c) { return (c>='a'&&c<='z') || (c>='A'&&c<='Z') || c=='_' || c=='.' || c=='$'; }
inline bool rest(char c) { return first(c) || (c>='0'&&c<='9'); }
inline bool identifier(std::string_view s) {
    if (s.empty() || s=="." || !first(s.front())) return false;
    for (char c:s) if (!rest(c)) return false;
    return true;
}
inline std::int64_t add(std::int64_t a,std::int64_t b) {
    if ((b>0 && a>INT64_MAX-b) || (b<0 && a<INT64_MIN-b)) throw std::invalid_argument("integer addition overflow");
    return a+b;
}
inline std::int64_t sub(std::int64_t a,std::int64_t b) {
    if ((b>0 && a<INT64_MIN+b) || (b<0 && a>INT64_MAX+b)) throw std::invalid_argument("integer subtraction overflow");
    return a-b;
}
inline std::uint64_t aligned(std::uint64_t value,std::uint64_t alignment) {
    if (!alignment || (alignment&(alignment-1))) throw std::invalid_argument("alignment must be a positive power of two");
    if (value>UINT64_MAX-(alignment-1)) throw std::invalid_argument("address overflow");
    return (value+alignment-1)&~(alignment-1);
}
inline bool fits(std::int64_t value,std::size_t width) {
    if (width==8) return true;
    auto bits=static_cast<unsigned>(width*8);
    return value>=-(INT64_C(1)<<(bits-1)) && (value<0 || static_cast<std::uint64_t>(value)<(UINT64_C(1)<<bits));
}
inline void store(std::vector<std::uint8_t>& data,std::size_t offset,std::size_t width,std::int64_t value,bool big) {
    if ((width!=1 && width!=2 && width!=4 && width!=8) || offset>data.size() || width>data.size()-offset)
        throw std::invalid_argument("invalid output field");
    if (!fits(value,width)) throw std::invalid_argument("value does not fit data field");
    auto bits=std::bit_cast<std::uint64_t>(value);
    for (std::size_t n=0;n<width;++n) data[offset+n]=static_cast<std::uint8_t>(bits>>(8*(big ? width-1-n : n)));
}
std::string hex(const std::vector<std::uint8_t>&);
} // namespace metaas::detail
