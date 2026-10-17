// Command-line driver for MetaChain (manual/10-text-and-cli.md). The text and
// binary commands are the embedding dispatcher, metachain::evaluate. The driver
// adds what only a process has: reading inputs and files, option parsing, the
// decode command and multi-file link.
//
// Exit status: 0 on success, 1 for failed processing or I/O, 2 for arguments.
#include "MetaChain.hpp"

#include <charconv>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr std::size_t max_input_bytes = 64u * 1024u * 1024u;
constexpr int exit_ok = 0, exit_failure = 1, exit_usage = 2;

// Commands whose result is bytes; they cannot be written to a terminal.
bool binary_result(std::string_view command) {
    return command == "encode" || command == "build" || command == "assemble" ||
           command == "object" || command == "link-assembly";
}
bool known_command(std::string_view command) {
    static constexpr std::string_view names[] = {
        "parse", "rewrite", "check", "run", "optimize", "encode", "decode", "native",
        "build", "assemble", "object", "link-assembly", "link"};
    for (auto name : names) if (name == command) return true;
    return false;
}

void usage(std::ostream& out) {
    out << "usage: metachain COMMAND [INPUT|-] [-o OUTPUT] [--no-optimize] [--max-steps N]\n"
           "       metachain link OBJECT... -o OUTPUT\n"
           "       metachain --help\n"
           "commands: parse rewrite check run optimize encode decode native build\n"
           "          assemble object link-assembly link\n";
}

struct Arguments {
    std::string command;
    std::vector<std::string> inputs;
    std::optional<std::string> output;
    bool no_optimize = false;
    std::optional<std::size_t> max_steps;
};

// Parses argv. Returns an error message on a usage mistake.
std::optional<std::string> parse_arguments(int argc, char** argv, Arguments& args, bool& help) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { help = true; return std::nullopt; }
        if (arg == "-o") {
            if (args.output) return "duplicate output option";
            if (i + 1 >= argc) return "missing value for -o";
            args.output = argv[++i];
        } else if (arg == "--no-optimize") {
            args.no_optimize = true;
        } else if (arg == "--max-steps") {
            if (args.max_steps) return "duplicate step limit option";
            if (i + 1 >= argc) return "missing value for --max-steps";
            const std::string value = argv[++i];
            std::size_t steps = 0;
            const auto* end = value.data() + value.size();
            const auto [ptr, ec] = std::from_chars(value.data(), end, steps);
            if (value.empty() || ec != std::errc{} || ptr != end) return "invalid step limit: " + value;
            args.max_steps = steps;
        } else if (arg.size() > 1 && arg[0] == '-') {
            return "unknown option: " + arg;
        } else if (args.command.empty()) {
            args.command = arg;
        } else {
            args.inputs.push_back(arg);
        }
    }
    return std::nullopt;
}

// Reads a whole input, bounded to max_input_bytes. "-" and an absent input both
// read standard input.
std::optional<std::string> read_input(const std::string& path, std::string& text) {
    std::string bytes;
    if (path.empty() || path == "-") {
        std::ostringstream all; all << std::cin.rdbuf();
        bytes = all.str();
    } else {
        std::ifstream in(path, std::ios::binary);
        if (!in) return "cannot read input file " + path;
        std::ostringstream all; all << in.rdbuf();
        if (in.bad()) return "cannot read input file " + path;
        bytes = all.str();
    }
    if (bytes.size() > max_input_bytes) return "input exceeds the 64 MiB limit";
    text = std::move(bytes);
    return std::nullopt;
}

std::optional<std::string> write_output(const std::string& path, const void* data, std::size_t size) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return "cannot write output file " + path;
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    if (!out) return "cannot write output file " + path;
    return std::nullopt;
}

int report(const std::vector<metachain::Diagnostic>& diagnostics) {
    for (const auto& d : diagnostics) std::cerr << "metachain: " << metachain::format(d) << '\n';
    return exit_failure;
}

int report_message(const std::string& message) {
    std::cerr << "metachain: " << message << '\n';
    return exit_failure;
}

int run_driver(const Arguments& args) {
    metachain::Options options;
    options.optimize = !args.no_optimize;
    if (args.max_steps) options.vm.max_steps = *args.max_steps;

    const std::string& command = args.command;
    if (command == "link") {
        if (args.inputs.empty()) { std::cerr << "metachain: link needs at least one object\n"; return exit_usage; }
        if (!args.output) { std::cerr << "metachain: link needs -o OUTPUT\n"; return exit_usage; }
        auto linked = metachain::Toolchain(options).link_files(args.inputs);
        if (!linked) return report(linked.diagnostics);
        if (auto error = write_output(*args.output, linked->data(), linked->size())) return report_message(*error);
        return exit_ok;
    }

    if (args.inputs.size() > 1) { std::cerr << "metachain: " << command << " accepts at most one input\n"; return exit_usage; }
    if (binary_result(command) && !args.output) {
        std::cerr << "metachain: " << command << " writes bytes; give -o OUTPUT\n";
        return exit_usage;
    }
    const std::string input = args.inputs.empty() ? std::string{} : args.inputs.front();

    std::string source;
    if (auto error = read_input(input, source)) return report_message(*error);

    if (command == "decode") {
        auto program = metachain::Toolchain(options).decode(
            std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(source.data()), source.size()));
        if (!program) return report(program.diagnostics);
        const std::string text = metail::emit(*program);
        if (args.output) {
            if (auto error = write_output(*args.output, text.data(), text.size())) return report_message(*error);
        } else {
            std::fwrite(text.data(), 1, text.size(), stdout);
        }
        return exit_ok;
    }

    auto result = metachain::evaluate(command, source, options);
    if (!result) return report(result.diagnostics);
    if (binary_result(command)) {
        if (auto error = write_output(*args.output, result.bytes.data(), result.bytes.size())) return report_message(*error);
        return exit_ok;
    }
    if (args.output) {
        if (auto error = write_output(*args.output, result.text.data(), result.text.size())) return report_message(*error);
    } else {
        std::fwrite(result.text.data(), 1, result.text.size(), stdout);
    }
    return exit_ok;
}
} // namespace

int main(int argc, char** argv) {
    Arguments args;
    bool help = false;
    if (auto error = parse_arguments(argc, argv, args, help)) {
        std::cerr << "metachain: " << *error << '\n';
        usage(std::cerr);
        return exit_usage;
    }
    if (help) { usage(std::cout); return exit_ok; }
    if (args.command.empty()) { usage(std::cerr); return exit_usage; }
    if (!known_command(args.command)) {
        std::cerr << "metachain: unknown command: " << args.command << '\n';
        usage(std::cerr);
        return exit_usage;
    }
    return run_driver(args);
}
