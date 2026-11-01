#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
namespace metavm { namespace machines {
/* JVM Code-array integer subset, not a .class loader. Every branch target must
 * be an instruction boundary. Values and local slots are signed Java int32. */
int32_t jvm_integer_method(const std::vector<uint8_t>& code,
    const std::vector<int32_t>& arguments={},size_t locals=16,size_t stack_limit=256,
    uint64_t fuel=1000000);
}}
