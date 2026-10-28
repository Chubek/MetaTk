#include <juniper/Juniper.hpp>
#include <charconv>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unistd.h>

namespace {
struct Failure { int status; std::string message; };
[[noreturn]] void fail(int status, std::string message) { throw Failure{status, std::move(message)}; }
template<class T> T take(juniper::Result<T> result) {
    if (!result) {
        const auto& d = *result.error();
        fail(d.code == "JUN-E205" ? 3 : 1, juniper::format(d));
    }
    return std::move(*result.get());
}
std::string help() {
    return "Juniper 0.1.0 — GPPX macro IR for MetaIL / MetaArch\n"
        "Usage: juniper COMMAND [FILE=Juniperfile] [options]\n"
        "Commands: preprocess, check, emit, metail, linkage, optimize, analyze, select, schedule\n"
        "  -o FILE             output (default stdout; '-' means stdout)\n"
        "  -I DIR              macro include directory (repeatable)\n"
        "  -D NAME[=VALUE]      define @NAME (repeatable)\n"
        "  --no-preprocess     read canonical IR directly (FILE may be '-')\n"
        "  --passes LIST       exact MetaIL passes, comma-separated (default all)\n"
        "  --rounds N          optimization rounds, 1..12 (default 2)\n"
        "  --optimize          optimize before select/schedule\n"
        "  --depfile FILE      write transitive Make/Ninja dependencies (requires -o)\n"
        "  --gppx FILE         preprocessor executable\n"
        "  --stdlib FILE       Juniper macro standard library\n"
        "  --metail-home DIR   real MetaIL runtime directory\n"
        "  --timeout N         GPPX deadline in seconds (default 10)\n"
        "  --                  end options\n"
        "Exit: 0 success, 1 invalid input, 2 usage, 3 I/O. Macro input must be trusted.\n";
}
unsigned number(const std::string& text, unsigned limit) {
    unsigned n = 0;
    auto [p, ec] = std::from_chars(text.data(), text.data() + text.size(), n);
    if (ec != std::errc{} || p != text.data() + text.size() || n == 0 || n > limit) fail(2, "invalid number: " + text);
    return n;
}
std::string read_source(const std::string& path) {
    std::ifstream file;
    std::istream* stream = &std::cin;
    if (path != "-") { file.open(path, std::ios::binary); if (!file) fail(3, "cannot read " + path); stream = &file; }
    std::string text;
    char buffer[8192];
    while (stream->read(buffer, sizeof(buffer)) || stream->gcount()) {
        if (text.size() + static_cast<std::size_t>(stream->gcount()) > 8 * 1024 * 1024) fail(1, "source exceeds 8 MiB");
        text.append(buffer, static_cast<std::size_t>(stream->gcount()));
    }
    if (stream->bad()) fail(3, "input read failed");
    return text;
}
bool aliases(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code ec;
    if (std::filesystem::equivalent(a, b, ec) && !ec) return true;
    ec.clear(); auto ca = std::filesystem::weakly_canonical(a, ec); if (ec) ca = std::filesystem::absolute(a).lexically_normal();
    ec.clear(); auto cb = std::filesystem::weakly_canonical(b, ec); if (ec) cb = std::filesystem::absolute(b).lexically_normal();
    return ca == cb;
}
void write_file(const std::filesystem::path& path, const std::string& data) {
    // Exclusive, same-directory temporary file + rename: semantic and write
    // failures do not truncate a previously successful artifact.
    auto pattern = path.string() + ".tmp.XXXXXX";
    std::vector<char> name(pattern.begin(), pattern.end()); name.push_back('\0');
    int fd = ::mkstemp(name.data());
    if (fd < 0) fail(3, "cannot create output beside " + path.string());
    struct Temp {
        int fd;
        std::string path;
        ~Temp() { if (fd >= 0) ::close(fd); if (!path.empty()) ::unlink(path.c_str()); }
    } temporary{fd, name.data()};
    std::size_t offset = 0;
    while (offset < data.size()) {
        auto count = ::write(fd, data.data() + offset, data.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) fail(3, "cannot write " + path.string());
        offset += static_cast<std::size_t>(count);
    }
    auto close_status = ::close(fd); temporary.fd = -1;
    if (close_status != 0) fail(3, "cannot close " + path.string());
    std::error_code ec; std::filesystem::rename(temporary.path, path, ec);
    if (ec) fail(3, "cannot replace " + path.string() + ": " + ec.message());
    temporary.path.clear();
}
std::string make_escape(const std::filesystem::path& path) {
    std::string out;
    for (char c : path.string()) {
        if (c == '\n' || c == '\r') fail(3, "newline in dependency path is not supported");
        if (c == '$') out += "$$";
        else { if (c == ' ' || c == '\t' || c == '#' || c == ':' || c == '\\') out += '\\'; out += c; }
    }
    return out;
}
int run(int argc, char** argv) {
    if (argc < 2) fail(2, help());
    std::string command = argv[1];
    if (command == "--help" || command == "-h") { std::cout << help(); return 0; }
    if (command == "--version") { std::cout << "Juniper 0.1.0\n"; return 0; }
    const std::set<std::string> commands{"preprocess", "check", "emit", "metail", "linkage", "optimize", "analyze", "select", "schedule"};
    if (!commands.contains(command)) fail(2, "unknown command: " + command);
    std::string input = "Juniperfile", output = "-", depfile, passes = "all", home;
    bool got_input = false, end_options = false, expand = true, optimize = false;
    bool got_passes = false, got_rounds = false;
    unsigned rounds = 2;
    juniper::PreprocessOptions options;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        auto value = [&]() -> std::string { if (++i >= argc) fail(2, "missing value for " + arg); return argv[i]; };
        if (!end_options && arg == "--") { end_options = true; continue; }
        if (!end_options && (arg == "--help" || arg == "-h")) { std::cout << help(); return 0; }
        if (!end_options && arg == "-o") output = value();
        else if (!end_options && arg == "--depfile") depfile = value();
        else if (!end_options && arg == "--no-preprocess") expand = false;
        else if (!end_options && arg == "--optimize") optimize = true;
        else if (!end_options && arg == "--passes") { passes = value(); got_passes = true; }
        else if (!end_options && arg == "--rounds") { rounds = number(value(), 12); got_rounds = true; }
        else if (!end_options && arg == "--timeout") options.timeout_seconds = number(value(), 3600);
        else if (!end_options && arg == "--gppx") options.executable = value();
        else if (!end_options && arg == "--stdlib") options.standard_library = value();
        else if (!end_options && arg == "--metail-home") home = value();
        else if (!end_options && arg.starts_with("-I")) options.include_directories.emplace_back(arg.size() == 2 ? value() : arg.substr(2));
        else if (!end_options && arg.starts_with("-D")) {
            auto definition = arg.size() == 2 ? value() : arg.substr(2);
            auto equal = definition.find('='); auto name = definition.substr(0, equal);
            options.defines[name] = equal == std::string::npos ? "1" : definition.substr(equal + 1);
        } else if (!end_options && arg.starts_with('-') && arg != "-") fail(2, "unknown option: " + arg);
        else { if (got_input) fail(2, "only one input is accepted"); input = arg; got_input = true; }
    }
    bool selection = command == "select" || command == "schedule";
    if (optimize && !selection) fail(2, "--optimize is only valid for select/schedule");
    if (got_rounds && command != "optimize" && !(selection && optimize)) fail(2, "--rounds requires optimization");
    if (got_passes && command != "analyze" && command != "optimize" && !(selection && optimize)) fail(2, "--passes requires analysis/optimization");
    if (command == "check" && (output != "-" || !depfile.empty())) fail(2, "check does not write artifacts");
    if (!depfile.empty() && output == "-") fail(2, "--depfile requires an output file");
    if (!expand && (!options.defines.empty() || !options.include_directories.empty())) fail(2, "-I/-D require preprocessing");
    if (expand && input == "-") fail(2, "stdin requires --no-preprocess");
    std::string source;
    std::vector<std::filesystem::path> dependencies;
    if (expand) {
        auto expanded = take(juniper::preprocess(input, options)); source = std::move(expanded.text); dependencies = std::move(expanded.dependencies);
    } else {
        source = read_source(input);
        if (input != "-") dependencies.push_back(std::filesystem::canonical(input));
    }
    for (const auto& p : dependencies) {
        if (output != "-" && aliases(p, output)) fail(3, "output aliases an input dependency: " + p.string());
        if (!depfile.empty() && aliases(p, depfile)) fail(3, "depfile aliases an input dependency: " + p.string());
    }
    if (output != "-" && !depfile.empty() && aliases(output, depfile)) fail(3, "output and depfile alias");
    std::string artifact;
    if (command == "preprocess") artifact = std::move(source);
    else {
        juniper::Compiler compiler(home.empty() ? nullptr : home.c_str());
        auto module = take(compiler.parse(source, input));
        if (command == "optimize" || optimize) module = take(compiler.optimize(module, passes, static_cast<int>(rounds)));
        if (command == "check") { (void)take(compiler.check(module)); return 0; }
        if (command == "emit" || command == "optimize") artifact = take(compiler.emit(module));
        else if (command == "metail") artifact = take(compiler.emit_metail(module));
        else if (command == "linkage") artifact = take(compiler.linkage(module));
        else if (command == "analyze") artifact = take(compiler.analyze(module, passes));
        else {
            auto machine = take(juniper::select(module, home.empty() ? nullptr : home.c_str()));
            if (command == "schedule") machine = take(juniper::schedule(machine));
            artifact = juniper::emit_machine_json(machine);
        }
    }
    // Compute/validate both artifacts before opening either destination.
    std::string deptext;
    if (!depfile.empty()) {
        deptext = make_escape(std::filesystem::absolute(output)) + ':';
        for (const auto& dependency : dependencies) deptext += " \\\n  " + make_escape(dependency);
        deptext += '\n';
    }
    if (output == "-") { std::cout << artifact; if (!std::cout) fail(3, "stdout write failed"); }
    else {
        if (!depfile.empty()) write_file(depfile, deptext);
        write_file(output, artifact);
    }
    return 0;
}
} // namespace
int main(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (const Failure& e) { std::cerr << e.message << '\n'; return e.status; }
    catch (const std::filesystem::filesystem_error& e) { std::cerr << e.what() << '\n'; return 3; }
    catch (const std::exception& e) { std::cerr << "juniper: " << e.what() << '\n'; return 1; }
}
