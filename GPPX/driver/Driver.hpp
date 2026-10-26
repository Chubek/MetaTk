#pragma once
#include "GPPX.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace gppx::driver {
void config(Engine&,const std::filesystem::path&);
void plugins(Engine&,const std::vector<std::filesystem::path>&);
int repl(Engine&);
}
