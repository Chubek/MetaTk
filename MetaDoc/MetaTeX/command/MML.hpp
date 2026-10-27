#pragma once
#include "Native.hpp"
namespace metatex::command {
std::string evaluateMML(std::string_view source, std::span<const std::string> arguments = {},
    const std::filesystem::path& filename = "<metaml>");
void registerMML(Engine& engine, std::string name, unsigned arity, std::string source,
    std::filesystem::path filename = "<metaml>", bool local = false);
}
