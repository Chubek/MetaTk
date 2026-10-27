#pragma once
#include "MetaTeX.h"
namespace metatex::command {
// Symbols insert literal Unicode text, never re-tokenized TeX source.
void registerSymbol(Engine& engine, std::string name, std::string utf8, bool local = false);
void installTextSymbols(Engine& engine);
}
