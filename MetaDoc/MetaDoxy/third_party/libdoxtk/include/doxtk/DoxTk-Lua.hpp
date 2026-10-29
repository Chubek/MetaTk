#pragma once
#include "doxtk/DoxTk.hpp"

// Optional Lua runtime integration.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkLua.cpp.
namespace doxtk {
struct LuaModuleInfo {
    static constexpr std::string_view description = "Optional Lua runtime integration.";
};
} // namespace doxtk
