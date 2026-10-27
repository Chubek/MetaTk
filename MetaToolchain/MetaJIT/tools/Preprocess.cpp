#include "Preprocess.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void usage(std::ostream &out) {
    out << "Usage: metajit-preprocess [-I DIR] [-D NAME=VALUE] [--metail-home DIR] [-o OUTPUT] INPUT\n";
}
}
int main(int argc, char **argv) {
    metajit::Preprocess::Options options;
    std::string input, output;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help") { usage(std::cout); return 0; }
        if (arg == "-I" || arg == "-D" || arg == "--metail-home" || arg == "-o") {
            if (++i == argc) { usage(std::cerr); return 2; }
            const std::string value = argv[i];
            if (arg == "-I") options.include_paths.emplace_back(value);
            else if (arg == "--metail-home") options.metail_home = value;
            else if (arg == "-o") output = value;
            else {
                const auto equal = value.find('=');
                options.definitions.emplace_back(value.substr(0, equal), equal == value.npos ? "1" : value.substr(equal + 1));
            }
        } else if (!arg.empty() && arg.front() == '-') { usage(std::cerr); return 2; }
        else if (!input.empty()) { usage(std::cerr); return 2; }
        else input = arg;
    }
    if (input.empty()) { usage(std::cerr); return 2; }
    try {
        const auto result = metajit::Preprocess(std::move(options)).file(input);
        if (output.empty()) {
            std::cout << result;
            if (!std::cout) throw std::runtime_error("cannot write standard output");
        } else {
            std::ofstream file(output, std::ios::binary | std::ios::trunc);
            file << result;
            file.close();
            if (!file) throw std::runtime_error("cannot write " + output);
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
