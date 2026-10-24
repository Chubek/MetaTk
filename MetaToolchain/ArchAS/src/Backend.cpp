#include "Internal.hpp"
#include "../../ArchBFD.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace archas {
namespace detail {
std::filesystem::path data_root(const std::filesystem::path &explicit_path, bool bfd) {
    if (!explicit_path.empty()) return explicit_path;
    const char *name = bfd ? "ArchBFD" : "MetaArchDB";
    if (const char *env = std::getenv(bfd ? "ARCHBFDDB" : "METAARCHDB")) if (*env) return env;
    for (const auto &root : {std::filesystem::path(name), std::filesystem::path("..") / name,
            std::filesystem::path(ARCHAS_INSTALL_DATA) / name, std::filesystem::path(ARCHAS_SOURCE_DATA) / name})
        if (std::filesystem::is_directory(root)) return root;
    throw Error(std::string(name) + " not found; specify its database directory");
}
std::string hex(std::string_view data) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    if (data.size() > out.max_size() / 2) throw Error("hex output too large");
    out.reserve(data.size() * 2);
    for (unsigned char b : data) { out += digits[b >> 4]; out += digits[b & 15]; }
    return out;
}
std::string json_string(std::string_view text) {
    std::string out = "\"";
    constexpr char digits[] = "0123456789abcdef";
    for (unsigned char c : text) {
        if (c == '\\' || c == '"') { out += '\\'; out += char(c); }
        else if (c < 32) { out += "\\u00"; out += digits[c >> 4]; out += digits[c & 15]; }
        else out += char(c);
    }
    return out + '"';
}
std::string image_json(const mair::Image &image, const mair::Target &target) {
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << "{\"schema\":1,\"target\":" << json_string(target.name()) << ",\"target_index\":" << target.index() << ",\"sections\":[";
    for (size_t i = 0; i < image.sections.size(); ++i) {
        const auto &s = image.sections[i];
        std::string data(s.bytes.begin(), s.bytes.end());
        o << (i ? "," : "") << "{\"name\":" << json_string(s.name) << ",\"alignment\":" << s.alignment << ",\"hex\":" << json_string(hex(data)) << '}';
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
          << ",\"symbol\":" << json_string(r.symbol) << ",\"section\":" << r.section << ",\"offset\":" << r.offset
          << ",\"size\":" << r.size << ",\"value\":" << r.value << ",\"bit_offset\":" << r.bit_offset
          << ",\"bit_width\":" << r.bit_width << ",\"byte_order\":" << json_string(r.endian == DSYM_ENDIAN_BIG ? "big" : "little")
          << ",\"pc_relative\":" << (r.pc_relative ? "true" : "false") << '}';
    }
    return o.str() + "]}\n";
}
namespace {
mair::Limits ir_limits(const Options &o) { return {o.limits.bytes, o.limits.statements, 64, o.limits.bytes}; }
using BFDSpec = std::unique_ptr<ArchBFDSpec, decltype(&archbfd_spec_free)>;
BFDSpec bfd_spec(const Options &options, bool bits64, bool big) {
    archbfd::api::Diagnostics diags;
    auto path = data_root(options.bfd_root, true) / (bits64 ? "ELF64.bfd.yaml" : "ELF32.bfd.yaml");
    BFDSpec spec(archbfd_spec_load_file(path.c_str(), DSYM_FORMAT_AUTO, 0, diags.get()), archbfd_spec_free);
    if (!spec) throw Error(diags.count() ? diags.copy().back().message : "cannot load ELF container description");
    // Container layouts are endian-neutral. This owned copy is configured before
    // any read; no shared catalogue/specification is mutated.
    spec->byte_order = big ? ARCHBFD_BO_BIG : ARCHBFD_BO_LITTLE;
    spec->has_byte_order = true;
    return spec;
}
std::string flatten(std::string_view data, const Options &options) {
    if (data.size() < 16 || data.substr(0, 4) != std::string_view("\177ELF", 4) ||
        (data[4] != 1 && data[4] != 2) || (data[5] != 1 && data[5] != 2))
        throw Error("native flat output requires an ELF object");
    auto spec = bfd_spec(options, data[4] == 2, data[5] == 2);
    archbfd::api::Diagnostics diags;
    std::unique_ptr<ArchBFDObject, decltype(&archbfd_object_close)> object(
        archbfd_object_open_memory(data.data(), data.size(), spec.get(), diags.get()), archbfd_object_close);
    if (!object) throw Error(diags.count() ? diags.copy().back().message : "invalid assembler object");
    if (archbfd_object_type(object.get()) != 1) throw Error("assembler did not produce a relocatable object");
    if (archbfd_object_reloc_count(object.get())) throw Error("flat output cannot contain unresolved relocations; emit an object and link it");
    const ArchBFDObjectSection *selected = nullptr;
    int64_t note = 0;
    if (!archbfd_constant(spec.get(), "section_types", "SHT_NOTE", &note)) throw Error("ELF description lacks SHT_NOTE");
    // ELF gABI SHF_ALLOC; the historical catalogue does not list this flag.
    constexpr uint64_t alloc = 0x2;
    for (size_t i = 0; i < archbfd_object_section_count(object.get()); ++i) {
        const auto *s = archbfd_object_section(object.get(), i);
        // GNU as can synthesize allocated GNU-property notes. They describe
        // object metadata, not the payload represented by a flat byte stream.
        if (!(s->flags & alloc) || !s->size || s->type == note) continue;
        if (selected) throw Error("flat output requires at most one nonempty allocated section");
        selected = s;
    }
    if (!selected) return {};
    int64_t nobits = 0;
    if (!archbfd_constant(spec.get(), "section_types", "SHT_NOBITS", &nobits)) throw Error("ELF description lacks SHT_NOBITS");
    if (selected->size < 0 || uint64_t(selected->size) > options.limits.bytes) throw Error("section exceeds output byte limit");
    if (selected->type == nobits) return std::string(size_t(selected->size), '\0');
    if (selected->offset < 0 || uint64_t(selected->offset) > data.size() || uint64_t(selected->size) > data.size() - size_t(selected->offset))
        throw Error("section outside assembler object");
    return std::string(data.substr(size_t(selected->offset), size_t(selected->size)));
}
struct TargetTools { const char *arch, *little, *big, *gas; };
constexpr TargetTools tool_targets[] = {
#define ARCHAS_TARGET(arch, little, big, gas) {arch, little, big, gas},
#include "Targets.def"
#undef ARCHAS_TARGET
};
const TargetTools *tool_target(const mair::Target &target) {
    for (const auto &entry : tool_targets) if (target.description().arch == std::string_view(entry.arch)) return &entry;
    return nullptr;
}
bool x86(const mair::Target &t) {
    return std::string_view(t.description().arch) == "amd64" || std::string_view(t.description().arch) == "x86";
}
#if !defined(_WIN32)
struct Temporary {
    std::filesystem::path path;
    Temporary() {
        auto text = (std::filesystem::temp_directory_path() / "domas-XXXXXX").string();
        if (!mkdtemp(text.data())) throw IOError("cannot create assembler temporary directory: " + std::string(std::strerror(errno)));
        path = text;
    }
    ~Temporary() { std::error_code error; std::filesystem::remove_all(path, error); }
};
std::string executable(std::string name) {
    auto valid = [](const std::filesystem::path &p) { return std::filesystem::is_regular_file(p) && access(p.c_str(), X_OK) == 0; };
    if (name.find('/') != name.npos) return valid(name) ? name : std::string{};
    const char *env = std::getenv("PATH");
    std::string_view path = env ? env : "/usr/bin:/bin";
    for (;;) {
        auto end = path.find(':');
        auto directory = path.substr(0, end);
        auto candidate = std::filesystem::path(directory.empty() ? "." : std::string(directory)) / name;
        if (valid(candidate)) return candidate.string();
        if (end == path.npos) break;
        path.remove_prefix(end + 1);
    }
    return {};
}
int run(const std::vector<std::string> &command, const std::filesystem::path &log, size_t limit) {
    std::vector<char *> argv;
    for (const auto &arg : command) {
        if (arg.find('\0') != arg.npos) throw Error("NUL in assembler argument");
        argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);
    int fd = open(log.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (fd < 0) throw IOError("cannot create assembler diagnostic file");
    pid_t child = fork();
    if (child < 0) { close(fd); throw IOError("cannot start assembler"); }
    if (!child) {
        if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) _exit(126);
        close(fd);
        rlimit resource{};
        if (getrlimit(RLIMIT_FSIZE, &resource) == 0) {
            resource.rlim_cur = std::min(resource.rlim_max, static_cast<rlim_t>(limit));
            if (setrlimit(RLIMIT_FSIZE, &resource) != 0) _exit(126);
        }
        execv(argv[0], argv.data());
        _exit(127);
    }
    close(fd);
    int status = 0;
    while (waitpid(child, &status, 0) < 0) if (errno != EINTR) throw IOError("waiting for assembler failed");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
}
std::string map_diagnostics(std::string text, const std::string &file, const Lines &lines) {
    size_t position = 0;
    while ((position = text.find(file + ":", position)) != text.npos) {
        auto start = position + file.size() + 1;
        size_t line = 0;
        auto r = std::from_chars(text.data() + start, text.data() + text.size(), line);
        if (r.ec != std::errc{} || line == 0 || line > lines.size()) { position = start; continue; }
        size_t end = size_t(r.ptr - text.data()), column = lines[line - 1].location.column;
        if (end + 1 < text.size() && text[end] == ':' && text[end + 1] >= '0' && text[end + 1] <= '9') {
            size_t generated_column = 0;
            auto c = std::from_chars(text.data() + end + 1, text.data() + text.size(), generated_column);
            if (c.ec == std::errc{}) end = size_t(c.ptr - text.data());
        }
        const auto &loc = lines[line - 1].location;
        auto replacement = loc.file + ":" + std::to_string(loc.line) + ":" + std::to_string(column);
        text.replace(position, end - position, replacement); position += replacement.size();
    }
    // GAS emits a filename-only header before its positioned diagnostics.
    while ((position = text.find(file)) != text.npos) text.replace(position, file.size(), "<assembly>");
    return text;
}
#endif
} // namespace

Lines lower(const Lines &input, const mair::Target &target, const Options &options) {
    Lines output;
    size_t size = 0;
    auto emit = [&](std::string text, const Location &loc) {
        if (size >= options.limits.bytes || text.size() > options.limits.bytes - size - 1) throw Error("output byte limit exceeded", loc);
        size += text.size() + 1; output.push_back({std::move(text), loc});
    };
    if (x86(target)) emit(options.syntax == Syntax::intel ? ".intel_syntax noprefix" : ".att_syntax prefix", {"<command-line>", 1, 1});
    BFDSpec relocations(nullptr, archbfd_spec_free);
    for (const auto &line : input) {
        auto [op, rest] = head(line.text);
        (void)rest;
        if (op != ".encode" && op != ".form") { emit(line.text, line.location); continue; }
        try {
            auto image = mair::assemble(mair::parse(line.text, line.location.file, ir_limits(options)), target, ir_limits(options));
            const auto &bytes = image.sections.at(0).bytes;
            if (!bytes.empty()) {
                std::string text = ".byte ";
                for (size_t i = 0; i < bytes.size(); ++i) text += (i ? "," : "") + std::to_string(bytes[i]);
                emit(std::move(text), line.location);
            }
            for (const auto &r : image.relocations) {
                // GAS .reloc cannot supply in-place REL addends. Only the
                // independently described amd64 ELF64 RELA bridge is enabled.
                if (std::string_view(target.description().arch) != "amd64" ||
                    std::string_view(target.description().obj_format) != "ELF64" ||
                    (!options.triple.empty() && options.triple.find("linux") == options.triple.npos))
                    throw Error("encoded relocation object lowering is available for amd64 ELF64; use native instructions or --emit json");
                if (!relocations) relocations = bfd_spec(options, true, false);
                auto index = archbfd_find_reloc(relocations.get(), r.name.c_str());
                if (index < 0) throw Error("relocation absent from ArchBFD: " + r.name);
                const auto &howto = relocations->relocations[index];
                if (r.size != uint64_t(howto.size) || r.bit_width != uint64_t(howto.bitsize) ||
                    r.bit_offset != uint64_t(howto.bitpos) || r.pc_relative != howto.pcrel ||
                    howto.rightshift || r.endian != DSYM_ENDIAN_LITTLE)
                    throw Error("Dsymspec/ArchBFD relocation field mismatch: " + r.name);
                std::string expression = r.symbol;
                if (expression.empty()) expression = std::to_string(r.value);
                else if (r.value) expression += (r.value > 0 ? "+" : "") + std::to_string(r.value);
                emit(".reloc .-" + std::to_string(bytes.size()) + "+" + std::to_string(r.offset) + ", " + r.name + ", " + expression, line.location);
            }
        } catch (const Error &e) { throw Error(e.what(), line.location); }
    }
    return output;
}

mair::Image builtin(const Lines &lines, const mair::Target &target, const Options &options) {
    mair::Program program;
    Constants constants = options.defines;
    size_t data_bytes = 0;
    auto data_room = [&](size_t n) {
        if (data_bytes > options.limits.bytes || n > options.limits.bytes - data_bytes) throw Error("output byte limit exceeded");
        data_bytes += n;
    };
    for (const auto &line : lines) {
        try {
            auto [op, rest] = head(line.text);
            if (line.text.ends_with(':')) program.label(line.text.substr(0, line.text.size() - 1));
            else if (op == ".text" || op == ".data" || op == ".bss") {
                if (!rest.empty()) throw Error("builtin backend does not support subsections");
                program.section(op);
            } else if (op == ".section") program.section(rest);
            else if (op == ".globl" || op == ".global") {
                for (const auto &name : arguments(rest)) program.global(name).at(line.location);
            } else if (op == ".set" || op == ".equ" || op == ".equiv") {
                auto args = arguments(rest); constants[args.at(0)] = evaluate(args.at(1), constants); continue;
            } else if (op == ".encode" || op == ".form") {
                auto parsed = mair::parse(line.text, line.location.file, ir_limits(options));
                (void)mair::assemble(parsed, target, ir_limits(options));
                const auto &s = parsed.statements().at(0);
                auto bindings = mair::detail::bindings(s.args);
                if (op == ".encode") program.encode(s.name, bindings);
                else program.form(size_t(mair::detail::integer(s.name)), bindings);
            } else if (op == ".ascii" || op == ".asciz" || op == ".string") {
                for (const auto &arg : arguments(rest)) {
                    auto bytes = mair::unquote(arg);
                    if (op != ".ascii") bytes += '\0';
                    data_room(bytes.size()); program.ascii(std::move(bytes)).at(line.location);
                }
            } else if (op == ".zero" || op == ".space" || op == ".skip") {
                auto args = arguments(rest);
                if (args.empty() || args.size() > 2) throw Error("expected count[, fill]");
                auto n = evaluate(args[0], constants), fill = args.size() == 2 ? evaluate(args[1], constants) : 0;
                if (n < 0 || uint64_t(n) > options.limits.bytes || fill < -128 || fill > 255) throw Error("invalid space count/fill");
                data_room(size_t(n)); program.ascii(std::string(size_t(n), char(static_cast<unsigned char>(fill))));
            } else if (op == ".balign" || op == ".p2align" || (op == ".align" && x86(target))) {
                auto n = evaluate(rest, constants);
                if (op == ".p2align") { if (n < 0 || n > 16) throw Error("alignment out of range"); n = int64_t(1) << unsigned(n); }
                if (n < 1 || n > 65536) throw Error("alignment out of range");
                program.align(unsigned(n));
            } else if (op == ".byte" || op == ".2byte" || op == ".4byte" || op == ".8byte" ||
                       op == ".short" || op == ".long" || op == ".quad" || (op == ".word" && x86(target))) {
                std::string directive = op == ".byte" ? "byte" : op == ".2byte" || op == ".short" || op == ".word" ? "word" :
                                        op == ".4byte" || op == ".long" ? "long" : "quad";
                auto args = arguments(rest);
                for (auto &value : args) value = std::to_string(evaluate(value, constants));
                program.data(directive, std::move(args));
            } else throw Error("builtin backend cannot assemble '" + op + "'; use .encode/.form or a native backend");
            program.at(line.location);
        } catch (const Error &e) { throw Error(e.what(), line.location); }
    }
    return mair::assemble(program, target, ir_limits(options));
}

Output native(const Lines &lines, const mair::Target &target, const Options &options) {
#if defined(_WIN32)
    (void)lines; (void)target; (void)options;
    throw Error("native assembler invocation currently requires POSIX; use the builtin backend");
#else
    Backend backend = options.backend;
    const auto *description = tool_target(target);
    std::string triple = options.triple;
    if (triple.empty() && description) triple = dsym_target_is_big_endian(&target.description()) ? description->big : description->little;
    std::string tool;
    if (!options.assembler.empty()) {
        tool = executable(options.assembler.string());
        if (tool.empty()) throw IOError("assembler executable not found: " + options.assembler.string());
        if (backend == Backend::automatic) throw Error("--assembler requires --backend gas or llvm");
    }
    if (backend == Backend::automatic) {
#if defined(__x86_64__) || defined(__i386__)
        if (x86(target) && options.triple.empty()) tool = executable("as");
#endif
        if (!tool.empty()) backend = Backend::gas;
        else { tool = executable("llvm-mc"); backend = Backend::llvm; }
    }
    if (backend == Backend::gas && tool.empty()) {
#if defined(__x86_64__) || defined(__i386__)
        if (x86(target) && options.triple.empty()) tool = executable("as");
#endif
        if (tool.empty() && description && *description->gas) tool = executable(std::string(description->gas) + "-as");
    }
    if (backend == Backend::llvm && tool.empty()) tool = executable("llvm-mc");
    if (tool.empty()) throw IOError("native assembler not found; install llvm-mc/GNU as or set --assembler");
    if (backend == Backend::llvm && triple.empty()) throw Error("no native target triple for " + target.name() + "; specify --triple");
    if (backend == Backend::gas && !options.triple.empty()) throw Error("--triple is an LLVM option; select a GNU cross assembler with --assembler");
    Temporary work;
    auto input = work.path / "input.s", object = work.path / "output.o", log = work.path / "diagnostics";
    std::string source;
    for (const auto &line : lines) { append(source, line.text, options.limits.bytes); append(source, "\n", options.limits.bytes); }
    write_file(input, source);
    std::vector<std::string> command{tool};
    if (backend == Backend::llvm) { command.push_back("-triple=" + triple); command.push_back("-filetype=obj"); }
    else if (x86(target)) command.push_back(target.description().addr_size == 64 ? "--64" : "--32");
    else if (std::string_view(target.description().arch).starts_with("ppc")) {
        command.push_back(target.description().addr_size == 64 ? "-a64" : "-a32");
        command.push_back(dsym_target_is_big_endian(&target.description()) ? "-mbig" : "-mlittle");
    } else if (std::string_view(target.description().arch).starts_with("mips"))
        command.push_back(dsym_target_is_big_endian(&target.description()) ? "-EB" : "-EL");
    command.insert(command.end(), options.assembler_args.begin(), options.assembler_args.end());
    command.insert(command.end(), {"-o", object.string(), input.string()});
    int status = run(command, log, options.limits.bytes);
    std::string diagnostics = map_diagnostics(read_file(log, options.limits.bytes), input.string(), lines);
    if (status == 126 || status == 127) throw IOError("cannot execute assembler: " + tool);
    if (status) throw Error("assembler exited with status " + std::to_string(status) + (diagnostics.empty() ? "" : ":\n" + diagnostics));
    auto data = read_file(object, options.limits.bytes);
    if (data.empty()) throw Error("assembler produced an empty object");
    if (options.format != Format::object) {
        data = flatten(data, options);
        if (options.format == Format::hex) data = hex(data) + '\n';
    }
    return {std::move(data), std::move(diagnostics), {}};
#endif
}
} // namespace detail

mair::Target load_target(std::string name, std::filesystem::path db, size_t index, std::filesystem::path spec_file) {
    static const std::map<std::string, std::string> aliases = {{"x86_64", "amd64"}, {"arm64", "aarch64"}, {"i386", "x86"}, {"rv64", "riscv64"}, {"rv32", "riscv32"}};
    if (auto it = aliases.find(name); it != aliases.end()) name = it->second;
    if (!mair::ident(name) || name == "." || name == "..") throw Error("invalid target name");
    if (spec_file.empty()) {
        auto root = detail::data_root(db, false);
        spec_file = root / name / (name + ".march");
        if (!std::filesystem::is_regular_file(spec_file)) spec_file = root / (name + ".dsymspec.yaml");
    }
    return mair::Target(mair::detail::checked(metaarch::Spec::load(spec_file.string())), name, index);
}
std::vector<std::string> targets(std::filesystem::path db) { return mair::targets(detail::data_root(db, false)); }
Assembler::Assembler(mair::Target target, Options options) : target_(std::move(target)), options_(std::move(options)) {
    (void)target_.spec();
    if (options_.syntax == Syntax::intel && std::string_view(target_.description().arch) != "amd64" && std::string_view(target_.description().arch) != "x86")
        throw Error("Intel syntax is only valid for x86/amd64 targets");
    if (!options_.limits.bytes || !options_.limits.statements || !options_.limits.expansion_depth) throw Error("limits must be positive");
    if (options_.format == Format::json && options_.backend != Backend::automatic && options_.backend != Backend::builtin)
        throw Error("JSON section images require the builtin backend");
    if (options_.backend == Backend::builtin && options_.format == Format::object) throw Error("builtin backend emits bin, hex, json or asm; object output requires gas/llvm");
}
Assembler &Assembler::directive(std::string name, Directive callback) {
    auto key = detail::directive_name(std::move(name));
    if (!callback || !directives_.emplace(key, std::move(callback)).second) throw Error("invalid or duplicate directive: " + key);
    return *this;
}
Output Assembler::compile(std::string_view source, const std::string &file) const {
    auto translation = detail::preprocess(source, file, target_, options_, directives_);
    Output result;
    if (options_.format == Format::json || (options_.backend == Backend::builtin && options_.format != Format::assembly)) {
        auto image = detail::builtin(translation.lines, target_, options_);
        if (options_.format == Format::json) result.data = detail::image_json(image, target_);
        else {
            auto bytes = image.flat(); result.data.assign(bytes.begin(), bytes.end());
            if (options_.format == Format::hex) result.data = detail::hex(result.data) + '\n';
        }
    } else {
        auto lines = detail::lower(translation.lines, target_, options_);
        if (options_.format == Format::assembly) for (const auto &line : lines) {
            detail::append(result.data, line.text, options_.limits.bytes); detail::append(result.data, "\n", options_.limits.bytes);
        }
        else result = detail::native(lines, target_, options_);
    }
    if (result.data.size() > options_.limits.bytes) throw Error("output byte limit exceeded");
    result.dependencies = std::move(translation.dependencies);
    return result;
}
std::string read_file(const std::filesystem::path &path, size_t limit) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw IOError("cannot open input: " + path.string());
    std::string out;
    char buffer[8192];
    while (in.read(buffer, sizeof buffer) || in.gcount()) detail::append(out, {buffer, size_t(in.gcount())}, limit);
    if (!in.eof()) throw IOError("read failed: " + path.string());
    return out;
}
void write_file(const std::filesystem::path &path, std::string_view data) {
#if defined(_WIN32)
    std::ofstream out(path, std::ios::binary);
    out.write(data.data(), std::streamsize(data.size())); out.close();
    if (!out) throw IOError("output write failed: " + path.string());
#else
    auto temporary = path.string() + ".XXXXXX";
    int fd = mkstemp(temporary.data());
    if (fd < 0) throw IOError("cannot create output: " + path.string() + ": " + std::strerror(errno));
    try {
        while (!data.empty()) {
            auto count = ::write(fd, data.data(), std::min(data.size(), size_t(std::numeric_limits<ssize_t>::max())));
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) throw IOError("output write failed: " + path.string());
            data.remove_prefix(size_t(count));
        }
        int status = close(fd); fd = -1;
        if (status) throw IOError("output close failed: " + path.string());
        if (rename(temporary.c_str(), path.c_str()) != 0) throw IOError("output replacement failed: " + path.string() + ": " + std::strerror(errno));
    } catch (...) {
        if (fd >= 0) close(fd);
        unlink(temporary.c_str()); throw;
    }
#endif
}
} // namespace archas
