#include "MML.hpp"
#include "../core/Scripting.h"
namespace metatex::command {
std::string evaluateMML(std::string_view source, std::span<const std::string> arguments, const std::filesystem::path& filename) {
    return detail::runMetaML(source, {arguments.begin(), arguments.end()}, filename);
}
void registerMML(Engine& engine, std::string name, unsigned arity, std::string source, std::filesystem::path filename, bool local) {
    registerNative(engine, std::move(name), arity,
        [source = std::move(source), filename = std::move(filename)](std::span<const std::string> arguments) {
            return evaluateMML(source, arguments, filename);
        }, ResultKind::TeX, local);
}
}
