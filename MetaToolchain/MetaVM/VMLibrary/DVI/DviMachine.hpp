#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace metavm { namespace machines {
/* Standard DVI format 2, in integer DVI units. No file/font discovery, rasterizer
 * or interpretation of driver-specific specials. See docs/DVI.md. */
class DviMachine {
public:
    struct Limits {
        size_t max_input_bytes=64*1024*1024;
        size_t max_instructions=1000000;
        size_t max_pages=65535;
        size_t max_fonts=4096;
        size_t max_stack=65535;
        size_t max_events=1000000;
        size_t max_special_bytes=16*1024*1024; // Aggregate, not per special.
    };
    struct Header {
        uint32_t numerator=0,denominator=0,magnification=0;
        int32_t max_height=0,max_width=0;
        uint16_t declared_stack=0;
        std::string comment;
    };
    struct Font {
        int64_t number=0; // fnt4/fnt_def4 numbers are signed, other widths unsigned.
        uint32_t checksum=0;
        int32_t scale=0,design_size=0;
        std::string area,name;
    };
    struct Position { int32_t h=0,v=0,w=0,x=0,y=0,z=0; };
    struct Page {
        std::array<int32_t,10> counts{};
        size_t begin_offset=0,end_offset=0;
    };
    enum class Kind { begin_page, glyph, rule, special, end_page };
    struct Event {
        Kind kind=Kind::begin_page;
        size_t source_offset=0,page=0;
        Position position;
        int64_t font=0,character=0;
        int32_t height=0,width=0;
        bool advances=false;
        std::string bytes; // Specials retain arbitrary bytes, including NUL.
    };
    struct Result {
        Header header;
        std::vector<Font> fonts;
        std::vector<Page> pages;
        std::vector<Event> events;
        size_t instructions=0,peak_stack=0;
    };
    /* Borrowed callback returns a character advance already scaled to DVI units.
     * Required only for SET characters. Negative/zero widths are valid. No
     * metrics are guessed. Callback references are valid only during the call.
     * All bytecode structure is checked before the first callback. */
    using Metrics=std::function<int32_t(const Font&,int64_t)>;
    /* Inputs are borrowed and never mutated. Results own all data. Independent
     * calls share no state; synchronize mutable metric providers yourself.
     * Malformed input, exhausted limits and coordinate overflow throw
     * std::runtime_error with a byte offset. Allocation failures propagate
     * std::bad_alloc; callback exceptions propagate unchanged. */
    static Result run(const std::vector<uint8_t>&,const Metrics& = Metrics{});
    static Result run(const std::vector<uint8_t>&,const Metrics&,const Limits&);
};
}}
