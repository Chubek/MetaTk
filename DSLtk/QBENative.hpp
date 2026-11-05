#pragma once
// Optional QBE compiler service ABI, shared by the C++ and D executors.
#include <cstddef>
#include <cstdint>

extern "C" {
struct MetamlQbeCall { std::uintptr_t function, data; };
// offsets has block_count + 1 entries and partitions calls into basic blocks.
// Status: 0 = success, 1 = unavailable/compiler failure, 2 = size limit.
int metaml_qbe_compile(const MetamlQbeCall *calls, const std::size_t *offsets,
                       std::size_t block_count, std::size_t limit, void **module,
                       char *error, std::size_t error_size) noexcept;
void *metaml_qbe_entry(void *module, std::size_t block) noexcept;
std::size_t metaml_qbe_size(void *module) noexcept;
void metaml_qbe_release(void *module) noexcept;
}
