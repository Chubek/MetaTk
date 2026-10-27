#pragma once
#include "MetaTeX.h"
#include <span>
namespace metatex::command {
enum class ResultKind { Text, TeX };
using NativeFunction = std::function<std::string(std::span<const std::string>)>;
void validateName(std::string_view name);
// Reads arity unexpanded braced arguments, invokes the function, and inserts
// text or scanned TeX. Exceptions become source-positioned engine diagnostics.
Engine::Handler nativeHandler(unsigned arity, NativeFunction function, ResultKind result = ResultKind::TeX);
void registerNative(Engine& engine, std::string name, unsigned arity, NativeFunction function,
    ResultKind result = ResultKind::TeX, bool local = false);
}
