#include "MetaArch.hpp"
#include <charconv>
#include <fstream>
#include <iostream>
#include <iterator>

static int report(const metaarch::Error &e) {
    for (const auto &d : e.diagnostics)
        std::cerr << d.file << ':' << d.line << ':' << d.column << ": " << d.message << '\n';
    return 1;
}
static int usage() {
    std::cerr << "usage:\n"
                 "  metaarch assemble SPEC SOURCE [-o BINARY] [--target N] [--bind ENCODER=PARAM,...]\n"
                 "  metaarch convert SPEC -f yaml|json|xml|sexpr [-o OUTPUT]\n"
                 "SPEC may be a .march DSL file or any existing architecture format.\n"
                 "Without -o, assemble prints hexadecimal bytes. SOURCE '-' reads stdin.\n";
    return 2;
}
int main(int argc, char **argv) {
    if (argc < 4) return usage();
    std::string command = argv[1], path = argv[2], source_path, output;
    DsymFormat format = DSYM_FORMAT_AUTO;
    metaarch::AssembleOptions options;
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) output = argv[++i];
        else if (arg == "-f" && i + 1 < argc) {
            format = dsym_format_from_name(argv[++i]);
            if (format == DSYM_FORMAT_AUTO) return usage();
        } else if (arg == "--target" && i + 1 < argc) {
            std::string_view n = argv[++i];
            auto r = std::from_chars(n.data(), n.data() + n.size(), options.target);
            if (r.ec != std::errc{} || r.ptr != n.data() + n.size()) return usage();
        } else if (arg == "--bind" && i + 1 < argc) {
            std::string binding = argv[++i];
            auto eq = binding.find('=');
            if (eq == std::string::npos || !eq) return usage();
            std::vector<std::string> params;
            std::string_view list(binding.data() + eq + 1, binding.size() - eq - 1);
            while (!list.empty()) {
                auto comma = list.find(',');
                auto param = list.substr(0, comma);
                if (param.empty()) return usage();
                params.emplace_back(param);
                if (comma == std::string_view::npos) break;
                list.remove_prefix(comma + 1);
                if (list.empty()) return usage();
            }
            options.bindings[binding.substr(0, eq)] = std::move(params);
        } else if ((arg == "-" || !arg.starts_with('-')) && source_path.empty()) source_path = arg;
        else return usage();
    }
    if (command != "assemble" && command != "convert") return usage();
    if ((command == "assemble" && source_path.empty()) ||
        (command == "convert" && (format == DSYM_FORMAT_AUTO || !source_path.empty()))) return usage();
    auto spec = metaarch::Spec::load(path);
    if (!spec) return report(*spec.error());
    for (const auto &d : spec.get()->diagnostics())
        std::cerr << d.file << ':' << d.line << ':' << d.column << ": " << d.message << '\n';
    std::string bytes;
    if (command == "convert") {
        auto result = spec.get()->write(format);
        if (!result) return report(*result.error());
        bytes = std::move(*result.get());
    } else {
        std::ifstream file;
        std::istream *input = &std::cin;
        if (source_path != "-") {
            file.open(source_path, std::ios::binary);
            if (!file) { std::cerr << "cannot open " << source_path << '\n'; return 1; }
            input = &file;
        }
        std::string source{std::istreambuf_iterator<char>(*input), {}};
        if (input->bad()) { std::cerr << "cannot read assembly\n"; return 1; }
        auto result = spec.get()->assemble(source, source_path, options);
        if (!result) return report(*result.error());
        if (!result.get()->relocations.empty()) {
            std::cerr << "raw binary output cannot represent relocations\n"; return 1;
        }
        if (output.empty()) {
            const char *hex = "0123456789abcdef";
            bool first = true;
            for (uint8_t b : result.get()->bytes) {
                if (!first) std::cout << ' ';
                first = false;
                std::cout << hex[b >> 4] << hex[b & 15];
            }
            std::cout << '\n';
            return std::cout ? 0 : 1;
        }
        if (!result.get()->bytes.empty())
            bytes.assign(reinterpret_cast<const char *>(result.get()->bytes.data()), result.get()->bytes.size());
    }
    if (output.empty()) { std::cout << bytes; return std::cout ? 0 : 1; }
    std::ofstream file(output, std::ios::binary);
    file.write(bytes.data(), std::streamsize(bytes.size()));
    file.close();
    if (!file) { std::cerr << "cannot write " << output << '\n'; return 1; }
    return 0;
}
