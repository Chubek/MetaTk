#include "Symbol.hpp"
#include "Native.hpp"
#include "../utils/UTF8.hpp"
#include <stdexcept>
namespace metatex::command {
void registerSymbol(Engine& engine, std::string name, std::string text, bool local) {
    validateName(name); util::validateUTF8(text);
    if (text.find('\0') != std::string::npos) throw std::invalid_argument("NUL in symbol text");
    auto handler = [text = std::move(text)](Engine& e) { e.addText(text); };
    if (local) engine.defineCommand(std::move(name), std::move(handler));
    else engine.registerCommand(std::move(name), std::move(handler));
}
void installTextSymbols(Engine& engine) {
    registerSymbol(engine, "TeX", "TeX"); registerSymbol(engine, "LaTeX", "LaTeX");
    registerSymbol(engine, "quad", "    "); registerSymbol(engine, "qquad", "        ");
    registerSymbol(engine, ",", " "); registerSymbol(engine, ";", " ");
}
}
