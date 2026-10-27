#pragma once
#include "Native.hpp"
#include "Shell.hpp"
namespace metatex::command {
std::string runExternal(const std::filesystem::path& script,
    const std::vector<std::string>& arguments = {}, const ProcessOptions& options = {});
void registerExternal(Engine& engine, std::string name, unsigned arity, std::filesystem::path script,
    ProcessOptions options = {}, bool local = false);
}
