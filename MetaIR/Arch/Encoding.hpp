#pragma once

#include <array>
#include <cstdint>
#include <string_view>

// The executable, bounded subset of an architecture specification. These are
// encoding mechanisms, not opcode tables; target data lives in stdspec/.
namespace symspec::encoding {
enum class Form {
    Fixed, Relative, Indirect, OpcodeRegister, Unary, RegisterRegister,
    Load, Store, MoveImmediate, ShiftCL, ShiftImmediate, ArithmeticImmediate
};
struct Register {
    std::string_view name;
    unsigned code, width;
};
constexpr unsigned width_mask(unsigned width) {
    return width == 8 ? 1u : width == 32 ? 2u : width == 64 ? 4u : 0u;
}
struct Encoding {
    std::string_view mnemonic;
    Form form;
    unsigned widths;
    std::array<std::uint8_t, 3> opcode;
    unsigned opcode_size;
    unsigned group = 0;
    unsigned reg_operand = 1;
    bool rex_w = true;
    unsigned relocation = 0;
    std::string_view implicit_register;
};
} // namespace symspec::encoding
