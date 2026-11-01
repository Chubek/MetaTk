#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace metavm { namespace machines {
/* Whitespace-tokenized signed i64 FORTH subset: colon definitions, recursion,
 * literals, + - * / mod dup drop swap over = <. No I/O or OS words. */
std::vector<int64_t> forth(const std::string& source,uint64_t fuel=1000000,size_t stack_limit=65536);
}}
