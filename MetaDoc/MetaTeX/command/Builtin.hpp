#pragma once
#include <string>
#include <string_view>
#include <vector>
namespace metatex::command {
// Sorted snapshot of commands installed by an unextended Engine.
const std::vector<std::string>& builtinCommandNames();
bool isBuiltin(std::string_view name);
}
