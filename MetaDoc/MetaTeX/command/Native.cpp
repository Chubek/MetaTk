#include "Native.hpp"
#include "../utils/UTF8.hpp"
#include <stdexcept>
namespace metatex::command {
void validateName(std::string_view name) {
    util::validateUTF8(name);
    if (name.empty() || name.find_first_of("\\{} \t\r\n\0", 0, 8) != std::string_view::npos)
        throw std::invalid_argument("command name must omit escape, whitespace and braces");
}
Engine::Handler nativeHandler(unsigned arity, NativeFunction function, ResultKind result) {
    if (arity > 9 || !function) throw std::invalid_argument("invalid native command arity or callback");
    return [arity, function = std::move(function), result](Engine& engine) {
        auto before = engine.document().diagnostics.size(); std::vector<std::string> arguments;
        for (unsigned i = 0; i < arity; ++i) arguments.push_back(engine.tokensText(engine.readArgument()));
        if (engine.document().diagnostics.size() != before) return;
        try {
            auto output = function(arguments);
            if (output.size() > 16u * 1024u * 1024u) throw std::length_error("native command output limit exceeded");
            util::validateUTF8(output);
            if (output.find('\0') != std::string::npos) throw std::invalid_argument("NUL in native command output");
            if (result == ResultKind::TeX) engine.insert(Lexer::scan(output));
            else engine.addText(std::move(output));
        } catch (const std::exception& error) { engine.report(std::string("native command: ") + error.what(), true); }
    };
}
void registerNative(Engine& engine, std::string name, unsigned arity, NativeFunction function, ResultKind result, bool local) {
    validateName(name); auto handler = nativeHandler(arity, std::move(function), result);
    if (local) engine.defineCommand(std::move(name), std::move(handler));
    else engine.registerCommand(std::move(name), std::move(handler));
}
}
