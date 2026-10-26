#ifndef GPPX_ROOT_CPP_UMBRELLA_H
#define GPPX_ROOT_CPP_UMBRELLA_H
/** \file GPPX.hpp
 * Convenience facade for source-tree embedding. Installed clients may use
 * <GPPX.hpp> (Engine), and <GPPX-all.h> for the auxiliary C helpers.
 */
#include "include/GPPX.hpp"
#include "GPPX.h"
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
namespace gppx {
/** A lightweight, fluent C++ facade; owns an Engine and all callback storage.
 * Paths may be relative to the calling process. The engine itself is movable
 * but cannot be copied; this class follows that rule.
 */
class Preprocessor {
    Engine engine_;
public:
    Preprocessor() = default;
    Preprocessor(const Preprocessor&) = delete;
    Preprocessor& operator=(const Preprocessor&) = delete;
    Preprocessor& option(std::string key,std::string value) {
        engine_.set(std::move(key),std::move(value));return *this;
    }
    Preprocessor& macro(std::string name,std::string value) {
        engine_.define(std::move(name),std::move(value));return *this;
    }
    Preprocessor& includes(std::filesystem::path dir) {
        engine_.includePath(std::move(dir));return *this;
    }
    Preprocessor& plugin(const std::filesystem::path& library) {
        engine_.loadPlugin(library);return *this;
    }
    std::string text(std::string_view input,std::string path="<memory>") {
        return engine_.process(input,std::move(path));
    }
    std::string file(const std::filesystem::path& path) {return engine_.processFile(path);}
    Engine& engine() noexcept {return engine_;}
    const Engine& engine() const noexcept {return engine_;}
};
}
#endif
