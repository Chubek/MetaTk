#include "../ArchAS.hpp"
#include <iostream>

namespace {
struct Usage : std::runtime_error { using runtime_error::runtime_error; };
constexpr const char *help = R"(usage: domas -T TARGET [options] INPUT
ArchAS: GAS/MetaArchIR assembly with Lua templates and directive plugins.
  -T, --target NAME         MetaArchDB architecture (e.g. amd64, aarch64, riscv64)
      --db DIRECTORY       MetaArchDB or ArchDB root (otherwise METAARCHDB)
      --spec FILE          Load an explicit .march or .dsymspec specification
      --target-index N     Target record within the specification (default 0)
      --intel-syntax       x86 Intel syntax, without register prefixes
      --att-syntax         x86 AT&T syntax (default)
  -o, --output FILE         Output (default INPUT.o; '-' means stdout)
      --emit FORMAT        obj (default), asm, bin, hex, json
  -S, -E                   Emit expanded/lowered assembly
      --backend NAME       auto (default), gas, llvm, builtin
      --assembler FILE     Native assembler executable; requires gas/llvm backend
      --assembler-arg ARG  Additional native assembler argument (repeatable)
      --triple TRIPLE      LLVM target triple override
      --bfd-db DIRECTORY  ArchBFD root (otherwise ARCHBFDDB)
  -I DIR                   Include search directory (repeatable)
  -D NAME[=INTEGER]        Define an absolute constant (default value 1)
      --lua FILE           Load Lua directive/template helpers (repeatable)
      --plugin FILE        Load a shared-library directive plugin (repeatable)
      --check              Compile and validate without writing output
      --list-targets       List database architectures
      --list-encoders      List target encoder names and parameters
      --list-forms         List target form indices, mnemonics and encoders
      --max-bytes N        Cumulative input, expansion and output limit (16777216)
      --max-statements N   Expansion work/statement limit (1000000)
      --max-depth N        Macro/include/directive recursion limit (64)
      --lua-memory N       Lua allocation budget in bytes (67108864)
      --lua-instructions N Lua VM instruction budget (10000000)
  -h, --help               Show help
      --version            Show version and compiled extensions
      --                   End options; INPUT '-' reads stdin
Exit status: 0 success, 1 invalid source/target, 2 usage, 3 I/O/tool failure.
)";
size_t number(std::string_view text) {
    size_t value = 0;
    auto r = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || r.ec != std::errc{} || r.ptr != text.data() + text.size() || !value) throw Usage("expected positive integer: " + std::string(text));
    return value;
}
bool aliases(const std::filesystem::path &a, const std::filesystem::path &b) {
    return std::filesystem::weakly_canonical(a) == std::filesystem::weakly_canonical(b) ||
           (std::filesystem::exists(a) && std::filesystem::exists(b) && std::filesystem::equivalent(a, b));
}
int run(int argc, char **argv) {
    archas::Options options;
    std::string arch, input, output, listing, format = "obj";
    std::filesystem::path db, spec;
    size_t index = 0;
    bool check = false, positional = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i], inline_value;
        bool assigned = false;
        if (!positional && arg.starts_with("--")) {
            auto equal = arg.find('=');
            if (equal != arg.npos) { inline_value = arg.substr(equal + 1); arg.resize(equal); assigned = true; }
        }
        auto value = [&]() -> std::string {
            if (assigned) { assigned = false; return inline_value; }
            if (++i >= argc) throw Usage("missing value for " + arg);
            return argv[i];
        };
        if (!positional && arg == "--") { positional = true; continue; }
        if (!positional && (arg == "-h" || arg == "--help")) { if (assigned) throw Usage("--help takes no value"); std::cout << help; return 0; }
        if (!positional && arg == "--version") {
            if (assigned) throw Usage("--version takes no value");
            std::cout << "ArchAS domas 1.0 (Lua " << (archas::has_lua() ? "enabled" : "disabled")
                      << ", plugins " << (archas::has_plugins() ? "enabled" : "disabled") << ")\n"; return 0;
        }
        if (!positional && (arg == "-T" || arg == "--target")) arch = value();
        else if (!positional && (arg == "-o" || arg == "--output")) output = value();
        else if (!positional && arg == "--db") db = value();
        else if (!positional && arg == "--spec") spec = value();
        else if (!positional && arg == "--target-index") { auto v = value(); index = v == "0" ? 0 : number(v); }
        else if (!positional && arg == "--intel-syntax") options.syntax = archas::Syntax::intel;
        else if (!positional && arg == "--att-syntax") options.syntax = archas::Syntax::att;
        else if (!positional && arg == "--emit") format = value();
        else if (!positional && (arg == "-S" || arg == "-E")) format = "asm";
        else if (!positional && arg == "--backend") {
            auto backend = value();
            if (backend == "auto") options.backend = archas::Backend::automatic;
            else if (backend == "gas") options.backend = archas::Backend::gas;
            else if (backend == "llvm") options.backend = archas::Backend::llvm;
            else if (backend == "builtin") options.backend = archas::Backend::builtin;
            else throw Usage("unknown backend: " + backend);
        } else if (!positional && arg == "--assembler") options.assembler = value();
        else if (!positional && arg == "--assembler-arg") options.assembler_args.push_back(value());
        else if (!positional && arg == "--triple") options.triple = value();
        else if (!positional && arg == "--bfd-db") options.bfd_root = value();
        else if (!positional && arg.starts_with("-I")) options.include_paths.emplace_back(arg.size() == 2 ? value() : arg.substr(2));
        else if (!positional && (arg.starts_with("-D") || arg == "--define")) {
            auto definition = arg == "--define" || arg.size() == 2 ? value() : arg.substr(2);
            auto equal = definition.find('=');
            auto name = definition.substr(0, equal);
            if (!mair::ident(name)) throw Usage("invalid constant name: " + name);
            int64_t n = 1;
            if (equal != definition.npos) {
                try { n = mair::detail::integer(std::string_view(definition).substr(equal + 1)); }
                catch (const mair::Error &) { throw Usage("invalid constant value: " + definition); }
            }
            if (!options.defines.emplace(name, n).second) throw Usage("duplicate constant: " + name);
        } else if (!positional && arg == "--lua") options.lua_files.emplace_back(value());
        else if (!positional && arg == "--plugin") options.plugins.emplace_back(value());
        else if (!positional && arg == "--check") check = true;
        else if (!positional && (arg == "--list-targets" || arg == "--list-encoders" || arg == "--list-forms")) {
            if (!listing.empty()) throw Usage("select only one listing mode");
            listing = arg;
        } else if (!positional && arg == "--max-bytes") options.limits.bytes = number(value());
        else if (!positional && arg == "--max-statements") options.limits.statements = number(value());
        else if (!positional && arg == "--max-depth") options.limits.expansion_depth = number(value());
        else if (!positional && arg == "--lua-memory") options.limits.lua_memory = number(value());
        else if (!positional && arg == "--lua-instructions") options.limits.lua_instructions = number(value());
        else if (!positional && arg != "-" && arg.starts_with('-')) throw Usage("unknown option: " + arg);
        else if (input.empty()) input = arg;
        else throw Usage("expected one input file");
        if (assigned) throw Usage("option takes no value: " + arg);
    }
    if (format == "obj") options.format = archas::Format::object;
    else if (format == "asm") options.format = archas::Format::assembly;
    else if (format == "bin") options.format = archas::Format::binary;
    else if (format == "hex") options.format = archas::Format::hex;
    else if (format == "json") options.format = archas::Format::json;
    else throw Usage("unknown output format: " + format);
    if (check && !output.empty()) throw Usage("--check cannot be combined with --output");
    if (!listing.empty() && (!input.empty() || !output.empty() || check)) throw Usage("listing mode does not accept input/output/--check");
    if (listing == "--list-targets") { for (const auto &name : archas::targets(db)) std::cout << name << '\n'; return 0; }
    if (arch.empty()) throw Usage("--target is required");
    if (listing.empty() && input.empty()) throw Usage("input is required");
    if (!options.assembler.empty() && options.backend != archas::Backend::gas && options.backend != archas::Backend::llvm)
        throw Usage("--assembler requires --backend gas or llvm");
    auto target = archas::load_target(arch, db, index, spec);
    if (listing == "--list-encoders") {
        const auto *s = target.spec().get();
        for (size_t i = 0; i < s->encoder_count; ++i) {
            std::cout << s->encoders[i].name;
            for (const auto &name : target.parameters(s->encoders[i].name)) std::cout << ' ' << name;
            std::cout << '\n';
        }
        return 0;
    }
    if (listing == "--list-forms") {
        const auto *s = target.spec().get();
        for (size_t i = 0; i < s->form_count; ++i) std::cout << i << '\t' << s->forms[i].mnemonic << '\t' << s->forms[i].encoder << '\n';
        return 0;
    }
    if (output.empty()) {
        if (input == "-") output = "-";
        else { auto path = std::filesystem::path(input); path.replace_extension(format == "obj" ? ".o" : format == "asm" ? ".s" : "." + format); output = path.string(); }
    }
    if (!check && input != "-" && output != "-" && aliases(input, output)) throw Usage("input and output must differ");
    std::string source;
    if (input == "-") {
        char buffer[8192];
        while (std::cin.read(buffer, sizeof buffer) || std::cin.gcount()) {
            size_t n = size_t(std::cin.gcount());
            if (source.size() > options.limits.bytes || n > options.limits.bytes - source.size()) throw archas::Error("source byte limit exceeded");
            source.append(buffer, n);
        }
        if (!std::cin.eof()) throw archas::IOError("stdin read failed");
    } else source = archas::read_file(input, options.limits.bytes);
    auto result = archas::Assembler(target, options).compile(source, input == "-" ? "<stdin>" : input);
    if (!result.diagnostics.empty()) std::cerr << result.diagnostics;
    if (check) return 0;
    if (output == "-") {
        std::cout.write(result.data.data(), std::streamsize(result.data.size())); std::cout.flush();
        if (!std::cout) throw archas::IOError("stdout write failed");
    } else {
        if (target.spec().get()->source_path) result.dependencies.emplace_back(target.spec().get()->source_path);
        for (const auto &path : result.dependencies) if (aliases(path, output)) throw Usage("output aliases an input dependency: " + path.string());
        archas::write_file(output, result.data);
    }
    return 0;
}
} // namespace
int main(int argc, char **argv) {
    try { return run(argc, argv); }
    catch (const Usage &e) { std::cerr << "domas: " << e.what() << " (see --help)\n"; return 2; }
    catch (const archas::IOError &e) { std::cerr << "domas: " << e.what() << '\n'; return 3; }
    catch (const std::filesystem::filesystem_error &e) { std::cerr << "domas: " << e.what() << '\n'; return 3; }
    catch (const archas::Error &e) {
        if (!e.location.file.empty()) std::cerr << e.location.file << ':' << e.location.line << ':' << e.location.column << ": ";
        else std::cerr << "domas: ";
        std::cerr << e.what() << '\n'; return 1;
    }
    catch (const std::exception &e) { std::cerr << "domas: " << e.what() << '\n'; return 1; }
}
