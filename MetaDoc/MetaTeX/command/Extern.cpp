#include "Extern.hpp"
#include <fstream>
#include <stdexcept>
namespace metatex::command {
std::string runExternal(const std::filesystem::path& script, const std::vector<std::string>& arguments, const ProcessOptions& options) {
    if (script.string().find('\0') != std::string::npos) throw std::invalid_argument("NUL in script path");
    std::ifstream file(script, std::ios::binary); char signature[2]{};
    if (!file.read(signature, 2) || signature[0] != '#' || signature[1] != '!')
        throw std::runtime_error("script must be readable and start with a shebang: " + script.string());
    return runProcess(script, arguments, options);
}
void registerExternal(Engine& engine, std::string name, unsigned arity, std::filesystem::path script, ProcessOptions options, bool local) {
    if (script.empty()) throw std::invalid_argument("empty script path");
    script = std::filesystem::absolute(script);
    registerNative(engine, std::move(name), arity,
        [script = std::move(script), options](std::span<const std::string> args) {
            return runExternal(script, {args.begin(), args.end()}, options);
        }, ResultKind::TeX, local);
}
}
