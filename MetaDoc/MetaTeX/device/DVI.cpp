#include "DVI.hpp"
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

namespace metatex::dvi {
namespace {
class Cursor {
public:
    explicit Cursor(std::span<const std::uint8_t> bytes) : data_(bytes) {}
    std::size_t position() const { return offset_; }
    bool eof() const { return offset_ == data_.size(); }
    std::size_t remaining() const { return data_.size() - offset_; }
    std::uint32_t u(unsigned length) {
        if (!length || length > 4 || length > remaining()) fail("truncated integer");
        std::uint32_t result = 0;
        for (unsigned i = 0; i != length; ++i) result = (result << 8) | data_[offset_++];
        return result;
    }
    std::int32_t s(unsigned length) {
        auto value = u(length);
        if (length < 4 && (value & (1u << (length * 8 - 1))))
            value |= ~((1u << (length * 8)) - 1);
        return static_cast<std::int32_t>(value);
    }
    std::string string(std::size_t length) {
        if (length > remaining()) fail("truncated byte string");
        std::string out(reinterpret_cast<const char*>(data_.data() + offset_), length);
        offset_ += length;
        return out;
    }
    void skip(std::size_t length) {
        if (length > remaining()) fail("truncated command payload");
        offset_ += length;
    }
    [[noreturn]] void fail(const std::string& reason) const {
        throw Error(reason + " at DVI byte " + std::to_string(offset_));
    }
private:
    std::span<const std::uint8_t> data_;
    std::size_t offset_ = 0;
};
} // namespace

Document parse(std::span<const std::uint8_t> bytes) {
    Cursor input(bytes);
    Document result;
    if (input.eof() || input.u(1) != 247) input.fail("missing DVI preamble");
    if (input.u(1) != 2) input.fail("unsupported DVI format version");
    result.numerator = input.u(4);
    result.denominator = input.u(4);
    result.magnification = input.u(4);
    if (!result.numerator || !result.denominator || !result.magnification)
        input.fail("invalid DVI measurement constants");
    result.comment = input.string(input.u(1));
    bool in_page = false, seen_post = false, seen_trailer = false;
    std::int32_t previous_page = -1;
    std::size_t depth = 0;
    while (!input.eof()) {
        const std::size_t at = input.position();
        const auto op = input.u(1);
        if (seen_post && op != 249 && !(op >= 243 && op <= 246))
            input.fail("only font definitions and post_post allowed after postamble");
        if (op <= 127 || (op >= 128 && op <= 131)) {
            if (!in_page) input.fail("character outside page");
            if (op >= 128) input.u(op - 127);
            ++result.pages.back().glyphs;
        } else if (op == 132 || op == 137) {
            if (!in_page) input.fail("rule outside page");
            input.skip(8);
        } else if (op >= 133 && op <= 136) {
            if (!in_page) input.fail("put character outside page");
            input.u(op - 132);
            ++result.pages.back().glyphs;
        } else if (op == 138) { // nop
        } else if (op == 139) {
            if (in_page || seen_post) input.fail("nested or misplaced BOP");
            Page page;
            page.offset = at;
            for (auto& value : page.counters) value = input.s(4);
            if (input.s(4) != previous_page) input.fail("broken previous-page pointer");
            if (at > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
                input.fail("DVI page offset too large");
            previous_page = static_cast<std::int32_t>(at);
            result.pages.push_back(std::move(page));
            in_page = true;
            depth = 0;
        } else if (op == 140) {
            if (!in_page || depth != 0) input.fail("EOP without balanced stack");
            in_page = false;
        } else if (op == 141) {
            if (!in_page) input.fail("PUSH outside page");
            ++depth;
            if (depth > 65535) input.fail("DVI stack too deep");
            if (depth > result.pages.back().max_stack_depth)
                result.pages.back().max_stack_depth = depth;
        } else if (op == 142) {
            if (!in_page || depth == 0) input.fail("DVI stack underflow");
            --depth;
        } else if (op >= 143 && op <= 146) {
            if (!in_page) input.fail("right movement outside page");
            input.skip(op - 142);
        } else if (op == 147 || op == 152 || op == 161 || op == 166) {
            if (!in_page) input.fail("movement register outside page");
        } else if (op >= 148 && op <= 151) {
            if (!in_page) input.fail("w movement outside page");
            input.skip(op - 147);
        } else if (op >= 153 && op <= 156) {
            if (!in_page) input.fail("x movement outside page");
            input.skip(op - 152);
        } else if (op >= 157 && op <= 160) {
            if (!in_page) input.fail("down movement outside page");
            input.skip(op - 156);
        } else if (op >= 162 && op <= 165) {
            if (!in_page) input.fail("y movement outside page");
            input.skip(op - 161);
        } else if (op >= 167 && op <= 170) {
            if (!in_page) input.fail("z movement outside page");
            input.skip(op - 166);
        } else if (op >= 171 && op <= 234) {
            if (!in_page) input.fail("font selection outside page");
        } else if (op >= 235 && op <= 238) {
            if (!in_page) input.fail("font selection outside page");
            input.skip(op - 234);
        } else if (op >= 239 && op <= 242) {
            if (!in_page) input.fail("special outside page");
            input.skip(input.u(op - 238));
        } else if (op >= 243 && op <= 246) {
            Font font;
            font.id = input.u(op - 242);
            font.checksum = input.u(4);
            font.scaled_size = input.u(4);
            font.design_size = input.u(4);
            const auto area_length = input.u(1);
            const auto name_length = input.u(1);
            font.area = input.string(area_length);
            font.name = input.string(name_length);
            result.fonts.push_back(std::move(font));
        } else if (op == 247) {
            input.fail("unexpected second preamble");
        } else if (op == 248) {
            if (in_page || seen_post) input.fail("premature or duplicate postamble");
            if (input.s(4) != previous_page) input.fail("postamble page pointer mismatch");
            if (result.numerator != input.u(4) || result.denominator != input.u(4) ||
                result.magnification != input.u(4)) input.fail("postamble units mismatch");
            result.max_height = input.u(4);
            result.max_width = input.u(4);
            result.maximum_stack_depth = static_cast<std::uint16_t>(input.u(2));
            if (input.u(2) != result.pages.size()) input.fail("postamble page count mismatch");
            for (const auto& page : result.pages)
                if (page.max_stack_depth > result.maximum_stack_depth)
                    input.fail("declared DVI stack too small");
            result.postamble_offset = at;
            seen_post = true;
        } else if (op == 249) {
            if (!seen_post || in_page) input.fail("post_post without postamble");
            if (input.u(4) != result.postamble_offset || input.u(1) != 2)
                input.fail("invalid post_post pointer or version");
            if (input.remaining() < 4) input.fail("missing DVI padding");
            while (!input.eof()) if (input.u(1) != 223) input.fail("invalid DVI padding");
            seen_trailer = true;
            break;
        } else {
            input.fail("invalid DVI opcode " + std::to_string(op));
        }
        if (in_page && op != 139) ++result.pages.back().operations;
    }
    if (!seen_trailer) input.fail("missing DVI post_post");
    return result;
}

Document parseFile(const std::string& path, std::size_t max_size) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw Error("cannot open DVI: " + path);
    file.seekg(0, std::ios::end);
    const auto length = file.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length) > max_size)
        throw Error("invalid or excessive DVI file size: " + path);
    file.seekg(0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if (length > 0 && !file.read(reinterpret_cast<char*>(bytes.data()), length))
        throw Error("cannot read DVI: " + path);
    return parse(bytes);
}
} // namespace metatex::dvi
