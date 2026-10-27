#pragma once
// TeX82 DVI (Device Independent) bytecode parser and structural verifier.
// This module depends only on ISO C++20; it does not interpret font metrics.
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace metatex::dvi {
struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
struct Font {
    std::uint32_t id = 0, checksum = 0, scaled_size = 0, design_size = 0;
    std::string area, name;
};
struct Page {
    std::size_t offset = 0;
    std::array<std::int32_t, 10> counters{};
    std::size_t operations = 0;
    std::size_t max_stack_depth = 0;
    std::size_t glyphs = 0;
};
struct Document {
    std::uint32_t numerator = 0, denominator = 0, magnification = 0;
    std::uint32_t max_height = 0, max_width = 0;
    std::uint16_t maximum_stack_depth = 0;
    std::string comment;
    std::vector<Font> fonts;
    std::vector<Page> pages;
    std::size_t postamble_offset = 0;
};
Document parse(std::span<const std::uint8_t> bytes);
Document parseFile(const std::string& path, std::size_t max_size = 64u * 1024u * 1024u);
} // namespace metatex::dvi
