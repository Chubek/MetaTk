#include "doxtk/DoxTk.hpp"
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    constexpr std::string_view name = "any2dvi";
    if (argc > 1 && (std::string_view(argv[1]) == "--help" ||
                     std::string_view(argv[1]) == "-h")) {
        std::cout << name << " — DoxTk document tool (scaffold)\n"
                  << "Usage: " << name << " [options] INPUT [OUTPUT]\n"
                  << "Conversion backend is not implemented yet.\n";
        return 0;
    }
    std::cerr << name << ": conversion backend is not implemented yet; "
              << "use --help for usage.\n";
    return 2;
}
