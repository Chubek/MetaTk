#include "doxtk/DoxTk-CLI.hpp"
#include "doxtk/DoxTk-Bytestream.hpp"
#include "doxtk/DoxTk-Driver.hpp"
#include "doxtk/DoxTk-Tooling.hpp"
#include <exception>
#include <iostream>
#include <set>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace doxtk {
Result<CLIOptions> parse_cli(std::span<const std::string_view> arguments, std::string_view fixed_target) {
    using Parsed = Result<CLIOptions>;
    CLIOptions options;
    options.conversion.source_format = "plaintext";
    options.conversion.target_format = fixed_target;
    std::vector<std::string_view> positional;
    std::set<std::string_view> seen;
    bool positional_only = false;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const auto argument = arguments[i];
        if (!positional_only && argument == "--") { positional_only = true; continue; }
        if (!positional_only && (argument == "--help" || argument == "-h")) { options.help = true; continue; }
        if (!positional_only && argument == "--version") { options.show_version = true; continue; }
        if (!positional_only && argument == "--list-formats") { options.list_formats = true; continue; }
        if (!positional_only && argument == "--allow-lossy") { options.conversion.allow_lossy = true; continue; }
        if (!positional_only && (argument == "--from" || argument == "--to" || argument == "--encoding")) {
            if (!seen.insert(argument).second) {
                return Parsed::failure(ErrorCode::invalid_argument, "Option specified more than once", std::string(argument));
            }
            if (i + 1 == arguments.size() || arguments[i + 1].empty() || arguments[i + 1].starts_with('-')) {
                return Parsed::failure(ErrorCode::invalid_argument, "Option requires a value", std::string(argument));
            }
            const auto value = arguments[++i];
            if (argument == "--from") options.conversion.source_format = value;
            else if (argument == "--encoding") options.conversion.source_encoding = value;
            else {
                if (!fixed_target.empty() && value != fixed_target) {
                    return Parsed::failure(ErrorCode::invalid_argument,
                        "This tool has a fixed output format; use doxtk-convert instead", std::string(value));
                }
                options.conversion.target_format = value;
            }
        } else if (!positional_only && argument.starts_with('-')) {
            return Parsed::failure(ErrorCode::invalid_argument, "Unknown option; use --help", std::string(argument));
        } else {
            positional.push_back(argument);
        }
    }
    if (options.help || options.show_version || options.list_formats) return Parsed::success(std::move(options));
    if (positional.empty() || positional.size() > 2 || positional[0].empty() ||
        (positional.size() == 2 && positional[1].empty())) {
        return Parsed::failure(ErrorCode::invalid_argument, "Expected INPUT and optional OUTPUT; use --help", "arguments");
    }
    if (options.conversion.target_format.empty()) {
        return Parsed::failure(ErrorCode::invalid_argument, "Specify --to; use --list-formats", "arguments");
    }
    options.input = std::string(positional[0]);
    if (positional.size() == 2) options.output = std::filesystem::path(std::string(positional[1]));
    return Parsed::success(std::move(options));
}

namespace {
void report_error(std::string_view name, const Error& error) {
    std::cerr << name << ": " << error_code_name(error.code) << ": " << error.message;
    if (!error.context.empty()) std::cerr << " (" << error.context << ')';
    std::cerr << '\n';
}
}

int run_conversion_cli(int argc, char** argv, std::string_view name, std::string_view fixed_target) {
    try {
        std::vector<std::string_view> arguments;
        for (int i = 1; i < argc; ++i) arguments.emplace_back(argv[i]);
        auto parsed = parse_cli(arguments, fixed_target);
        if (!parsed) { report_error(name, parsed.error); return 2; }
        const auto& options = *parsed.value;
        if (options.help) {
            std::cout << name << " — DoxTk document conversion\nUsage: " << name
                << " [--from FORMAT] " << (fixed_target.empty() ? "--to FORMAT " : "")
                << "[--encoding ENCODING] INPUT [OUTPUT]\n"
                << "Without OUTPUT, writes to standard output. Use -- before filenames beginning with '-'.\n"
                << "Input: plaintext, markdown (subset). Output: plaintext, json-dump, xml-dump, ir-json, ir-xml, html.\n"
                << "Encodings: utf-8 (default), ascii, iso-8859-1. Output is always UTF-8.\n"
                << "--allow-lossy permits structural loss (required for Markdown -> plaintext).\n"
                << "--help, --version, --list-formats\n";
            if (!fixed_target.empty()) std::cout << "Fixed output format: " << fixed_target << '\n';
            return 0;
        }
        if (options.show_version) { std::cout << "DoxTk " << version << '\n'; return 0; }
        if (options.list_formats) {
            for (const auto& format : builtin_formats()) {
                std::cout << format.name << "\tIR=" << format.ir << '\t'
                    << (format.readable ? (format.writable ? "read/write" : "read-only") : "write-only") << '\n';
            }
            return 0;
        }
        auto input = read_file(options.input);
        if (!input) { report_error(name, input.error); return 1; }
        auto output = convert(*input.value, options.conversion);
        if (!output) { report_error(name, output.error); return 1; }
        for (const auto& diagnostic : output.diagnostics) std::cerr << name << ": warning: " << diagnostic << '\n';
        if (options.output) {
            if (const auto error = write_file(*options.output, *output.value)) {
                report_error(name, error); return 1;
            }
        } else {
#ifdef _WIN32
            if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
                report_error(name, {ErrorCode::io_error, "Cannot set binary output mode", "stdout"}); return 1;
            }
#endif
            const auto text = bytes_to_string(*output.value);
            std::cout << text;
            std::cout.flush();
            if (!std::cout) {
                report_error(name, {ErrorCode::io_error, "Failed while writing output", "stdout"}); return 1;
            }
        }
        return 0;
    } catch (const std::exception& exception) {
        report_error(name, {ErrorCode::internal_error, exception.what(), "unexpected exception"});
        return 1;
    }
}
} // namespace doxtk
