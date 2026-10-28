#include "../MetaArchIR.hpp"
#include <iostream>

namespace {
struct Usage : std::runtime_error { using runtime_error::runtime_error; };
struct IO : std::runtime_error { using runtime_error::runtime_error; };
const char *help = R"(usage: marchir -T TARGET [options] INPUT
Portable assembly through MetaArchDB. INPUT or OUTPUT '-' means stdin/stdout.
  -T, --target NAME       MetaArchDB architecture directory
      --db DIRECTORY     Database root (otherwise METAARCHDB or ./MetaArchDB)
      --target-index N   Target record within the specification (default 0)
  -o, --output FILE      Output path (default INPUT.s, .bin, .hex or .json)
      --emit FORMAT      asm (default), bin, hex, json image, or linkage manifest
      --check            Validate selected output mode without writing
      --list-targets     List database architecture names
      --list-encoders    List encoder names and their named parameters
      --list-forms       List form indices, mnemonics and encoders
      --list-registers   List physical register names, widths and encodings
      --max-bytes N      Input/output byte limit (default 16777216)
  -h, --help             Show this help
      --version          Show language version
      --                 End options
Exit status: 0 success, 1 invalid source/target, 2 usage, 3 I/O failure.
)";
size_t number(const std::string &s) {
    size_t n = 0;
    auto r = std::from_chars(s.data(), s.data() + s.size(), n);
    if (s.empty() || r.ec != std::errc{} || r.ptr != s.data() + s.size()) throw Usage("invalid nonnegative integer: " + s);
    return n;
}
std::string json_string(std::string_view s) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 32 || c >= 127) { out += "\\u00"; out += digits[c >> 4]; out += digits[c & 15]; }
        else out += char(c);
    }
    return out + '"';
}
std::string hex(const std::vector<uint8_t> &bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for (auto b : bytes) { out += digits[b >> 4]; out += digits[b & 15]; }
    return out;
}
std::string image_json(const mair::Image &image, const mair::Target &target) {
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << "{\"schema\":1,\"target\":" << json_string(target.name()) << ",\"target_index\":" << target.index() << ",\"sections\":[";
    for (size_t i = 0; i < image.sections.size(); ++i) {
        const auto &s = image.sections[i];
        o << (i ? "," : "") << "{\"name\":" << json_string(s.name) << ",\"alignment\":" << s.alignment << ",\"hex\":" << json_string(hex(s.bytes)) << '}';
    }
    o << "],\"symbols\":[";
    for (size_t i = 0; i < image.symbols.size(); ++i) {
        const auto &s = image.symbols[i];
        o << (i ? "," : "") << "{\"name\":" << json_string(s.name) << ",\"section\":" << s.section << ",\"offset\":" << s.offset << ",\"global\":" << (s.global ? "true" : "false") << '}';
    }
    o << "],\"relocations\":[";
    for (size_t i = 0; i < image.relocations.size(); ++i) {
        const auto &r = image.relocations[i];
        o << (i ? "," : "") << "{\"name\":" << json_string(r.name) << ",\"parameter\":" << json_string(r.parameter)
          << ",\"symbol\":" << json_string(r.symbol) << ",\"section\":" << r.section << ",\"offset\":" << r.offset << ",\"size\":" << r.size
          << ",\"value\":" << r.value << ",\"bit_offset\":" << r.bit_offset << ",\"bit_width\":" << r.bit_width
          << ",\"byte_order\":" << json_string(r.endian == DSYM_ENDIAN_BIG ? "big" : "little")
          << ",\"pc_relative\":" << (r.pc_relative ? "true" : "false") << '}';
    }
    return o.str() + "],\"linkage\":" + mair::linkage_json(image.imports) + "}\n";
}
std::string read(std::istream &in, size_t limit) {
    std::string out;
    char buffer[8192];
    while (in.read(buffer, sizeof buffer) || in.gcount()) {
        size_t n = size_t(in.gcount());
        if (out.size() > limit || n > limit - out.size()) throw mair::Error("source byte limit exceeded");
        out.append(buffer, n);
    }
    if (!in.eof()) throw IO("input read failed");
    return out;
}
int run(int argc, char **argv) {
    std::string arch, input, output, format = "asm", listing;
    std::filesystem::path db;
    mair::Limits limits;
    size_t index = 0;
    bool check = false, positional = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto value = [&]() -> std::string { if (++i >= argc) throw Usage("missing value for " + a); return argv[i]; };
        if (!positional && a == "--") { positional = true; continue; }
        if (!positional && (a == "--help" || a == "-h")) { std::cout << help; return 0; }
        if (!positional && a == "--version") { std::cout << "MetaArchIR 1\n"; return 0; }
        if (!positional && (a == "-T" || a == "--target")) arch = value();
        else if (!positional && (a == "-o" || a == "--output")) output = value();
        else if (!positional && a == "--db") db = value();
        else if (!positional && a == "--emit") format = value();
        else if (!positional && a == "--target-index") index = number(value());
        else if (!positional && a == "--max-bytes") limits.source_bytes = limits.output_bytes = number(value());
        else if (!positional && a == "--check") check = true;
        else if (!positional && (a == "--list-targets" || a == "--list-forms" || a == "--list-encoders" || a == "--list-registers")) {
            if (!listing.empty()) throw Usage("only one listing mode may be selected");
            listing = a;
        } else if (!positional && a != "-" && a.starts_with('-')) throw Usage("unknown option: " + a);
        else if (input.empty()) input = a;
        else throw Usage("multiple input files");
    }
    if (format != "asm" && format != "bin" && format != "hex" && format != "json" && format != "linkage") throw Usage("unknown output format: " + format);
    if (!listing.empty() && (!input.empty() || !output.empty() || check)) throw Usage("listing mode does not accept input, output or --check");
    if (listing == "--list-targets") {
        for (const auto &name : mair::targets(db)) std::cout << name << '\n';
        return 0;
    }
    if (arch.empty()) throw Usage("--target is required");
    if (listing.empty() && input.empty()) throw Usage("input is required");
    if (check && !output.empty()) throw Usage("--check cannot be combined with --output");
    auto target = mair::target(arch, db, index);
    const auto *spec = target.spec().get();
    if (listing == "--list-encoders") {
        for (size_t i = 0; i < spec->encoder_count; ++i) {
            const auto &e = spec->encoders[i];
            std::cout << e.name;
            if (e.program) for (size_t j = 0; j < e.program->param_count; ++j) std::cout << (j ? ", " : " ") << e.program->params[j];
            std::cout << '\n';
        }
    } else if (listing == "--list-forms") {
        for (size_t i = 0; i < spec->form_count; ++i) std::cout << i << '\t' << spec->forms[i].mnemonic << '\t' << spec->forms[i].encoder << '\t' << spec->forms[i].operand_count << '\n';
    } else if (listing == "--list-registers") {
        for (size_t i = 0; i < spec->register_count; ++i) std::cout << spec->registers[i].name << '\t' << spec->registers[i].width << '\t' << spec->registers[i].enc << '\n';
    }
    if (!listing.empty()) return 0;
    std::string source;
    if (input == "-") source = read(std::cin, limits.source_bytes);
    else { std::ifstream in(input, std::ios::binary); if (!in) throw IO("cannot open input: " + input); source = read(in, limits.source_bytes); }
    auto program = mair::parse(source, input == "-" ? "<stdin>" : input, limits);
    std::string result;
    if (format == "asm") result = mair::render(program, target, limits);
    else if (format == "linkage") {
        result = mair::linkage_json(mair::external_functions(program)) + '\n';
    }
    else {
        auto image = mair::assemble(program, target, limits);
        if (format == "json") result = image_json(image, target);
        else {
            auto bytes = image.flat();
            if (format == "hex") result = hex(bytes) + '\n';
            else result.assign(bytes.begin(), bytes.end());
        }
    }
    if (result.size() > limits.output_bytes) throw mair::Error("output byte limit exceeded");
    if (check) return 0;
    if (output.empty()) {
        if (input == "-") output = "-";
        else { auto path = std::filesystem::path(input); path.replace_extension(format == "asm" ? ".s" : format == "linkage" ? ".link.json" : "." + format); output = path.string(); }
    }
    if (output == "-") { std::cout.write(result.data(), std::streamsize(result.size())); std::cout.flush(); if (!std::cout) throw IO("stdout write failed"); }
    else {
        if (input != "-" && (std::filesystem::weakly_canonical(input) == std::filesystem::weakly_canonical(output) ||
            (std::filesystem::exists(output) && std::filesystem::equivalent(input, output)))) throw Usage("input and output must differ");
        std::ofstream out(output, std::ios::binary);
        if (!out) throw IO("cannot open output: " + output);
        out.write(result.data(), std::streamsize(result.size())); out.close();
        if (!out) throw IO("write failed: " + output);
    }
    return 0;
}
} // namespace

extern "C" int marchir_main(int argc, char **argv) {
    try { return run(argc, argv); }
    catch (const Usage &e) { std::cerr << "marchir: " << e.what() << " (see --help)\n"; return 2; }
    catch (const IO &e) { std::cerr << "marchir: " << e.what() << '\n'; return 3; }
    catch (const std::filesystem::filesystem_error &e) { std::cerr << "marchir: " << e.what() << '\n'; return 3; }
    catch (const mair::Error &e) {
        if (!e.location.file.empty()) std::cerr << e.location.file << ':' << e.location.line << ':' << e.location.column << ": ";
        else std::cerr << "marchir: ";
        std::cerr << e.what() << '\n'; return 1;
    }
    catch (const std::exception &e) { std::cerr << "marchir: " << e.what() << '\n'; return 1; }
}
