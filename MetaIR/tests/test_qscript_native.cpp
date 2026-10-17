#include "qscript/include/QScript-Backend.h"
#include "qscript/include/QScript-Native.h"
#include "qscript/src/Runtime.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#if defined(__linux__)
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << __LINE__ << ": " #__VA_ARGS__ "\n"; std::exit(1); } } while (false)
namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;

static Bytes read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(input), {});
}
static void write(const fs::path& path, const Bytes& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.close(); CHECK(out);
}
static void static_image(const Bytes& bytes) {
    using namespace ld_tk::detail;
    Ehdr header{}; CHECK(read_at(bytes, 0, header));
    CHECK(header.type == ET_EXEC && header.machine == EM_X86_64);
    for (unsigned i = 0; i < header.phnum; ++i) {
        Phdr ph{}; CHECK(read_at(bytes, header.phoff + i * sizeof(Phdr), ph));
        CHECK(ph.type != 2 && ph.type != 3); // no dynamic table or ELF interpreter
        CHECK(ph.type != PT_LOAD || (ph.flags & 3) != 3);
        CHECK(ph.type != 0x6474e551 || !(ph.flags & PF_X));
    }
}
#if defined(__linux__)
static void execute(const fs::path& root, const Bytes& bytes, int expected, std::string_view output, std::string_view error = {}) {
    static_image(bytes);
    const auto exe = root / "native-good", out = root / "stdout", err = root / "stderr";
    write(exe, bytes); fs::permissions(exe, fs::perms::owner_all);
    posix_spawn_file_actions_t actions;
    CHECK(posix_spawn_file_actions_init(&actions) == 0);
    CHECK(posix_spawn_file_actions_addopen(&actions, 1, out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600) == 0);
    CHECK(posix_spawn_file_actions_addopen(&actions, 2, err.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600) == 0);
    char* args[] = {const_cast<char*>(exe.c_str()), nullptr};
    char path[] = "PATH=", language[] = "LANG=C";
    char* environment[] = {path, language, nullptr}; // no compiler, qscript, runtime paths or source resolver
    pid_t pid;
    CHECK(posix_spawn(&pid, exe.c_str(), &actions, nullptr, args, environment) == 0);
    posix_spawn_file_actions_destroy(&actions);
    int status; while (waitpid(pid, &status, 0) < 0) CHECK(errno == EINTR);
    const auto stdout_bytes = read(out), stderr_bytes = read(err);
    const std::string stdout_text(stdout_bytes.begin(), stdout_bytes.end()), stderr_text(stderr_bytes.begin(), stderr_bytes.end());
    if (!WIFEXITED(status) || WEXITSTATUS(status) != expected || stdout_text != output ||
        (error.empty() ? !stderr_text.empty() : stderr_text.find(error) == std::string::npos)) {
        std::cerr << "native status " << status << " stdout: " << stdout_text << " stderr: " << stderr_text << '\n';
        CHECK(false);
    }
}
static int fake_driver(int argc, char** argv) {
    const std::string mode = argv[1];
    if (mode == "--fake-timeout") {
        const auto child = fork(); CHECK(child >= 0);
        if (child == 0) { for (;;) pause(); }
        { std::ofstream pid(argv[2]); pid << child; }
        for (;;) pause();
    }
    if (mode == "--fake-noise") {
        const std::string block(4096, 'x');
        for (unsigned i = 0; i < 256; ++i) std::cout << block;
        return 7;
    }
    fs::path destination;
    for (int i = 2; i + 1 < argc; ++i) if (std::string_view(argv[i]) == "-o") destination = argv[i + 1];
    CHECK(!destination.empty());
    if (mode == "--fake-dynamic" || mode == "--fake-rwx" || mode == "--fake-interpreter" || mode == "--fake-exec-stack") {
        auto bytes = read(argv[2]); CHECK(bytes.size() > 18);
        if (mode == "--fake-dynamic") { bytes[16] = 3; bytes[17] = 0; } // ET_DYN
        else {
            using namespace ld_tk::detail;
            Ehdr header{}; CHECK(read_at(bytes, 0, header));
            bool changed = false;
            for (unsigned i = 0; i < header.phnum && !changed; ++i) {
                const auto offset = header.phoff + i * sizeof(Phdr);
                Phdr ph{}; CHECK(read_at(bytes, offset, ph));
                if (mode == "--fake-exec-stack" ? ph.type == 0x6474e551 : ph.type == PT_LOAD) {
                    if (mode == "--fake-interpreter") ph.type = 3;
                    else ph.flags |= PF_W | PF_X;
                    std::memcpy(bytes.data() + offset, &ph, sizeof(ph)); changed = true;
                }
            }
            CHECK(changed);
        }
        write(destination, bytes);
    } else write(destination, Bytes{'n','o','t',' ','E','L','F'});
    return 0;
}
#endif
int main(int argc, char** argv) {
#if defined(__linux__)
    if (argc > 1 && std::string_view(argv[1]).starts_with("--fake-")) return fake_driver(argc, argv);
    CHECK(argc == 2);
    CHECK(qs_native_default_driver() && qs_native_default_runtime());
    std::string pattern = (fs::temp_directory_path() / "qscript native 'spaces-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end()); name.push_back(0);
    CHECK(mkdtemp(name.data()));
    struct Scratch { fs::path root; ~Scratch() { std::error_code error; fs::remove_all(root, error); } } scratch{name.data()};
    const auto root = scratch.root;
    const auto source = R"QS(
        grammar G { token TOP { <word>+ % ',' } token word { <[a..z]>+ } }
        let g = G.extend("token word { <[A..Z]>+ }");
        let m = g.parse("ABC,DEF");
        print m.child("word").text();
        print m.find_all("word").len();
        print G.parse("x,y").child("word").text();
        return 42;
    )QS";
    auto unit = qscript::compile_unit(source); CHECK(unit);
    auto assembly = qscript::select(*unit); CHECK(assembly);
    CHECK(assembly->find(".global qs_native_entry") != std::string::npos);
    CHECK(assembly->find(".extern qs_native_dispatch") != std::string::npos);
    CHECK(assembly->find(".global _start") == std::string::npos);
    auto object = qscript::assemble(*assembly); CHECK(object);
    CHECK(qscript::detail::native_runtime_object(*object));
    auto linked = qscript::link({*object});
    if (!linked) for (const auto& d : linked.diagnostics) std::cerr << d.message << '\n';
    CHECK(linked); execute(root, *linked, 42, "ABC\n2\nx\n");

    // SysV defines only eax for an int return. A successful dispatcher may
    // leave the upper half of rax nonzero; verify status width and both ends
    // of the reversed-frame-to-contiguous-register snapshot independently.
    qscript::Unit abi;
    abi.program.code = {{metail::Op::Const,0,0,0,7}, {metail::Op::Const,31,0,0,99},
                        {metail::Op::Host,1,0,0,qscript::detail::builtin_id(qscript::detail::Builtin::CompileGrammar)},
                        {metail::Op::Halt,1,0,0,0}};
    auto abi_source = qscript::select(abi); CHECK(abi_source);
    auto abi_object = qscript::assemble(*abi_source); CHECK(abi_object);
    auto dispatch = qscript::assemble(R"ASM(.global qs_native_dispatch
qs_native_dispatch:
mov rax, [rdx]
cmp rax, 7
jne bad_abi
mov rax, [rdx+248]
cmp rax, 99
jne bad_abi
mov rax, 123
mov [rcx], rax
mov rax, 0x1234567800000000
ret
bad_abi:
mov eax, 126
ret
)ASM"); CHECK(dispatch);
    qscript::Options abi_options; abi_options.native_link.runtime_archive = argv[1];
    auto abi_image = qscript::link({*abi_object, *dispatch}, abi_options); CHECK(abi_image);
    execute(root, *abi_image, 123, "");
    ld_tk::detail::InputObject incompatible;
    incompatible.bytes = *object;
    std::vector<ld_tk::Diagnostic> object_errors;
    CHECK(ld_tk::detail::parse_object(incompatible, object_errors));
    bool changed_abi = false;
    for (const auto& symbol : incompatible.symbols) if (symbol.name == "qs_native_unit_data") {
        const auto offset = incompatible.sections[symbol.s.shndx].h.offset + symbol.s.value;
        incompatible.bytes.at(static_cast<std::size_t>(offset)) = QS_NATIVE_ABI + 1;
        changed_abi = true;
    }
    CHECK(changed_abi);
    auto incompatible_image = qscript::link({incompatible.bytes}); CHECK(incompatible_image);
    execute(root, *incompatible_image, 126, "", "invalid native runtime ABI");

    // Both archive and scratch paths contain whitespace/quotes, with no shell.
    qscript::Options options;
    fs::copy_file(qs_native_default_runtime(), root / "parser runtime 'quoted'.a");
    fs::create_directory(root / "C++ tools");
    fs::create_symlink(qs_native_default_driver(), root / "C++ tools" / "g++");
    options.native_link.driver = (root / "C++ tools" / "g++").string();
    options.native_link.runtime_archive = (root / "parser runtime 'quoted'.a").string();
    options.native_link.temporary_directory = root.string();
    auto built = qscript::build(*unit, options); CHECK(built);
    execute(root, *built, 42, "ABC\n2\nx\n");
    auto relocated = options; relocated.toolchain.linker.image_base = 0x600000;
    auto relocated_image = qscript::build(*unit, relocated); CHECK(relocated_image);
    execute(root, *relocated_image, 42, "ABC\n2\nx\n");

    // C configuration, emission and transactional rejection are additive.
    auto* context = qs_create(); CHECK(context);
    qs_native_link_options config{};
    CHECK(qs_get_native_link(context, &config) == QS_OK && config.timeout_seconds == 60);
    config = {options.native_link.driver.c_str(), options.native_link.runtime_archive.c_str(), root.c_str(), 10, 512};
    CHECK(qs_set_native_link(context, &config) == QS_OK);
    config.timeout_seconds = 3601;
    CHECK(qs_set_native_link(context, &config) == QS_INVALID_ARGUMENT);
    CHECK(qs_get_native_link(context, &config) == QS_OK && config.timeout_seconds == 10);
    auto* program = qs_compile(context, source, std::strlen(source)); CHECK(program);
    const unsigned char* data = nullptr; size_t length = 0;
    CHECK(qs_emit(context, program, QS_OBJECT, &data, &length) == QS_OK && length > 64);
    CHECK(qscript::detail::native_runtime_object(Bytes(data, data + length)));
    CHECK(qs_emit(context, program, QS_EXECUTABLE, &data, &length) == QS_OK);
    execute(root, Bytes(data, data + length), 42, "ABC\n2\nx\n");
    qs_program_destroy(program); qs_destroy(context);

    for (unsigned failure = 0; failure < 4; ++failure) {
        auto limited = options;
        if (failure == 0) limited.toolchain.vm.max_steps = 1;
        if (failure == 1) limited.toolchain.vm.max_stack_values = 0;
        if (failure == 2) limited.max_heap_objects = 1;
        if (failure == 3) limited.max_heap_bytes = 1;
        auto image = qscript::build(*unit, limited); CHECK(image);
        execute(root, *image, failure == 0 ? 124 : 125, "",
                failure == 2 ? "heap object limit exceeded" : failure == 3 ? "heap size limit exceeded" : "");
    }
    auto empty = qscript::compile_unit("let ms: list<match> = []; print ms; return 9;"); CHECK(empty);
    auto empty_image = qscript::build(*empty); CHECK(empty_image); execute(root, *empty_image, 9, "[]\n");
    auto invalid = qscript::compile_unit("let g=compile_grammar(\"not a grammar\"); print g.name();"); CHECK(invalid);
    auto invalid_image = qscript::build(*invalid); CHECK(invalid_image); execute(root, *invalid_image, 126, "", "invalid grammar:");

    const auto fails = [&](qscript::Options o, std::string_view message) {
        auto result = qscript::link({*object}, std::move(o));
        CHECK(!result && !result.value && !result.diagnostics.empty());
        CHECK(result.diagnostics[0].stage == metachain::Stage::Link);
        if (result.diagnostics[0].message.find(message) == std::string::npos) std::cerr << result.diagnostics[0].message << '\n';
        CHECK(result.diagnostics[0].message.find(message) != std::string::npos);
        for (const auto& file : fs::directory_iterator(root)) CHECK(!file.path().filename().string().starts_with("qscript-native-"));
        return result.diagnostics[0].message;
    };
    auto bad = options; bad.native_link.driver = (root / "missing driver").string(); fails(bad, "cannot execute");
    bad = options; bad.native_link.runtime_archive += ".missing"; fails(bad, "archive is unavailable");
    bad = options; bad.native_link.temporary_directory = (root / "missing directory").string(); fails(bad, "private native linker scratch");
    bad = options; bad.native_link.driver = std::string("bad\0driver", 10); fails(bad, "invalid native linker argument");
    bad = options; bad.native_link.timeout_seconds = 0; fails(bad, "resource limits");
    bad = options; bad.toolchain.linker.max_input_size = 1; fails(bad, "input size limit");
    bad = options; bad.toolchain.linker.max_output_size = 128; fails(bad, "output size limit");
    bad = options; bad.toolchain.linker.entry_symbol = "bad"; fails(bad, "CRT _start");

    const auto helper = root / "fake linker 'quoted'";
    fs::create_symlink(fs::canonical(argv[0]), helper);
    bad = options; bad.native_link.driver = helper.string(); bad.native_link.max_diagnostic_bytes = 256;
    bad.native_link.arguments = {"--fake-noise"};
    const auto noise = fails(bad, "output truncated"); CHECK(noise.size() < 1024);
    bad.native_link.arguments = {"--fake-invalid"}; fails(bad, "did not produce a static");
    write(root / "native-template", *linked);
    for (auto mode : {"--fake-dynamic", "--fake-rwx", "--fake-interpreter", "--fake-exec-stack"}) {
        bad.native_link.arguments = {mode, (root / "native-template").string()}; fails(bad, "did not produce a static");
    }
    CHECK(prctl(PR_SET_CHILD_SUBREAPER, 1) == 0);
    bad.native_link.timeout_seconds = 1;
    bad.native_link.arguments = {"--fake-timeout", (root / "timed-child.pid").string()};
    fails(bad, "timed out");
    pid_t descendant = -1; { std::ifstream input(root / "timed-child.pid"); input >> descendant; } CHECK(descendant > 0);
    int status; while (waitpid(descendant, &status, 0) < 0) CHECK(errno == EINTR);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL);

    // Invalid host bindings are never disguised as parser built-ins.
    auto host = *unit;
    for (auto& instruction : host.program.code) if (instruction.op == metail::Op::Host) { instruction.immediate = 123; break; }
    CHECK(!qscript::select(host));
    auto small = qscript::compile_unit("print \"standalone\"; return 7;"); CHECK(small);
    options.native_link.driver = "/missing/driver";
    auto small_image = qscript::build(*small, options); CHECK(small_image);
    execute(root, *small_image, 7, "standalone\n");
    std::cout << "Native grammar/match execution, static ELF, C ABI, limits and linker supervision passed\n";
#else
    (void)argc; (void)argv;
#endif
}
