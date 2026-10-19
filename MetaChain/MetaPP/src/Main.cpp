#include "MetaPP.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {
void help() {
    std::cout << "Usage: metapp [options] [input|-]\n"
                 "  -I DIR       Add an include search directory\n"
                 "  -D NAME[=V]  Define a macro (default value: 1)\n"
                 "  -U NAME      Undefine a macro\n"
                 "  -o FILE      Write output to FILE (default: stdout)\n"
                 "  -E, -P       Preprocess only; omit line markers (default)\n"
                 "  --version    Print version\n"
                 "  -h, --help   Print this help\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        metapp::Options options;
        std::vector<std::pair<char, std::string>> definitions;
        std::string input;
        std::string output;
        bool positional = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (!positional && (arg == "--help" || arg == "-h")) { help(); return 0; }
            if (!positional && arg == "--version") { std::cout << "MetaPP 0.1.0\n"; return 0; }
            if (!positional && arg == "--") { positional = true; continue; }
            if (!positional && (arg == "-E" || arg == "-P")) continue;
            if (!positional && arg.size() >= 2 && (arg[1] == 'I' || arg[1] == 'D' || arg[1] == 'U' || arg[1] == 'o') && arg[0] == '-') {
                std::string value = arg.substr(2);
                if (value.empty()) {
                    if (++i == argc) throw std::invalid_argument("missing argument for " + arg);
                    value = argv[i];
                }
                if (arg[1] == 'I') options.include_paths.emplace_back(value);
                else if (arg[1] == 'o') output = value;
                else definitions.emplace_back(arg[1], std::move(value));
            } else {
                if (!positional && arg != "-" && arg.starts_with('-')) throw std::invalid_argument("unknown option: " + arg);
                if (!input.empty()) throw std::invalid_argument("only one input file is supported");
                input = std::move(arg);
            }
        }
        metapp::Preprocessor pp(std::move(options));
        for (const auto& [kind, value] : definitions) {
            if (kind == 'D') pp.define(value);
            else pp.undefine(value);
        }
        std::string result;
        if (input.empty() || input == "-") {
            std::string source{std::istreambuf_iterator<char>(std::cin), {}};
            if (std::cin.bad()) throw std::runtime_error("cannot read stdin");
            result = pp.process(source, "<stdin>");
        } else result = pp.process_file(input);
        for (const auto& warning : pp.warnings())
            std::cerr << warning.location.file << ':' << warning.location.line << ": warning: " << warning.message << '\n';
        if (output.empty() || output == "-") {
            std::cout << result;
            if (!std::cout) throw std::runtime_error("cannot write stdout");
        } else {
            std::ofstream file(output, std::ios::binary);
            if (!file) throw std::runtime_error("cannot open output file: " + output);
            file << result;
            file.close();
            if (!file) throw std::runtime_error("cannot write output file: " + output);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "metapp: " << error.what() << '\n';
        return 1;
    }
}
