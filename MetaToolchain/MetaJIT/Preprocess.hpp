#ifndef METAJIT_PREPROCESS_HPP
#define METAJIT_PREPROCESS_HPP

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace metajit {

/** Build-time GPPX/MetaIL frontend. Each call uses an independent preprocessing
 * engine. Const calls may run concurrently if Options is not being modified.
 * Returned C++ owns its text; failures throw std::exception-derived errors.
 * No generated code is executed by this class. */
class Preprocess {
public:
    struct Options {
        std::string metail_home; ///< Empty selects MetaIL's runtime discovery.
        std::vector<std::filesystem::path> include_paths;
        std::vector<std::pair<std::string, std::string>> definitions;
    };

    Preprocess() = default;
    explicit Preprocess(Options options) : options_(std::move(options)) {}
    std::string process(std::string_view source,
                        const std::string &name = "<memory>") const;
    std::string file(const std::filesystem::path &path) const;

private:
    Options options_;
};

} // namespace metajit
#endif
