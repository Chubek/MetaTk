#include "qscript/src/Runtime.h"
#include "qscript/src/Internal.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#if defined(__linux__)
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef QSCRIPT_NATIVE_LINK_DRIVER
#define QSCRIPT_NATIVE_LINK_DRIVER ""
#endif
#ifndef QSCRIPT_NATIVE_RUNTIME_ARCHIVE
#define QSCRIPT_NATIVE_RUNTIME_ARCHIVE ""
#endif

extern "C" const char* qs_native_default_driver() {
    return QSCRIPT_NATIVE_LINK_DRIVER[0] ? QSCRIPT_NATIVE_LINK_DRIVER : nullptr;
}
extern "C" const char* qs_native_default_runtime() {
    return QSCRIPT_NATIVE_RUNTIME_ARCHIVE[0] ? QSCRIPT_NATIVE_RUNTIME_ARCHIVE : nullptr;
}

namespace qscript::detail {
namespace {
Result<std::vector<std::uint8_t>> failure(std::string message) {
    return {std::nullopt, {{metachain::Stage::Link, std::move(message), {}, 0, 0, 0}}};
}
#if defined(__linux__)
struct ProcessResult { int code = -1; std::string output, error; };
ProcessResult run_driver(std::vector<std::string> command, const NativeLinkOptions& options) {
    ProcessResult result;
    std::vector<char*> argv;
    for (auto& arg : command) argv.push_back(arg.data());
    argv.push_back(nullptr);
    struct Pipe {
        int fd[2]{-1, -1};
        ~Pipe() { for (int f : fd) if (f >= 0) ::close(f); }
    } pipe;
    if (::pipe2(pipe.fd, O_CLOEXEC) || ::fcntl(pipe.fd[0], F_SETFL, O_NONBLOCK) < 0) {
        result.error = "cannot create linker output pipe"; return result;
    }
    for (auto& fd : pipe.fd) if (fd <= 2) {
        const int replacement = ::fcntl(fd, F_DUPFD_CLOEXEC, 3);
        if (replacement < 0) { result.error = "cannot reserve linker pipe descriptors"; return result; }
        ::close(fd); fd = replacement;
    }
    struct Actions {
        posix_spawn_file_actions_t value{}; bool valid = false;
        ~Actions() { if (valid) posix_spawn_file_actions_destroy(&value); }
    } actions;
    struct Attributes {
        posix_spawnattr_t value{}; bool valid = false;
        ~Attributes() { if (valid) posix_spawnattr_destroy(&value); }
    } attributes;
    if (posix_spawn_file_actions_init(&actions.value)) { result.error = "cannot initialize linker process"; return result; }
    actions.valid = true;
    if (posix_spawnattr_init(&attributes.value)) { result.error = "cannot initialize linker attributes"; return result; }
    attributes.valid = true;
    if (posix_spawn_file_actions_adddup2(&actions.value, pipe.fd[1], 1) ||
        posix_spawn_file_actions_adddup2(&actions.value, pipe.fd[1], 2) ||
        posix_spawn_file_actions_addclose(&actions.value, pipe.fd[0]) ||
        posix_spawn_file_actions_addclose(&actions.value, pipe.fd[1]) ||
        posix_spawnattr_setflags(&attributes.value, POSIX_SPAWN_SETPGROUP) ||
        posix_spawnattr_setpgroup(&attributes.value, 0)) {
        result.error = "cannot configure linker process"; return result;
    }
    struct Child {
        pid_t pid = -1; bool reaped = false;
        ~Child() {
            if (pid > 0 && !reaped) {
                // A caller-installed SIGCHLD handler may have reaped it. Never
                // signal a potentially recycled process-group id in that case.
                int status;
                const auto waited = ::waitpid(pid, &status, WNOHANG);
                if (waited == pid || (waited < 0 && errno == ECHILD)) return;
                ::kill(-pid, SIGKILL);
                while (::waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
            }
        }
    } child;
    const int spawned = ::posix_spawnp(&child.pid, argv[0], &actions.value, &attributes.value, argv.data(), ::environ);
    if (spawned) { child.pid = -1; result.error = "cannot execute native linker driver: " + std::string(std::strerror(spawned)); return result; }
    ::close(pipe.fd[1]); pipe.fd[1] = -1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(options.timeout_seconds);
    bool eof = false, truncated = false;
    int status = 0;
    while (!child.reaped || !eof) {
        if (std::chrono::steady_clock::now() >= deadline) {
            ::kill(-child.pid, SIGKILL); // includes collect2/ld descendants
            result.error = "native linker timed out"; return result;
        }
        pollfd descriptor{pipe.fd[0], POLLIN | POLLHUP, 0};
        const int ready = ::poll(&descriptor, eof ? 0 : 1, 20);
        if (ready < 0 && errno != EINTR) { result.error = "reading native linker output failed"; return result; }
        if (!eof && ready > 0) {
            // Bound each drain as well as retained output: a noisy subprocess
            // must not prevent timeout checks or fill an unbounded log file.
            for (unsigned block = 0; block < 16; ++block) {
                char buffer[4096];
                const auto n = ::read(pipe.fd[0], buffer, sizeof buffer);
                if (n == 0) { eof = true; break; }
                if (n < 0) {
                    if (errno != EAGAIN && errno != EINTR) { result.error = "reading native linker output failed"; return result; }
                    break;
                }
                const auto size = static_cast<std::size_t>(n);
                const auto keep = std::min(size, options.max_diagnostic_bytes - result.output.size());
                result.output.append(buffer, keep); truncated = truncated || keep != size;
            }
        }
        if (!child.reaped) {
            const auto waited = ::waitpid(child.pid, &status, WNOHANG);
            if (waited == child.pid) child.reaped = true;
            else if (waited < 0 && errno != EINTR) {
                if (errno == ECHILD) child.reaped = true;
                result.error = "waiting for native linker failed"; return result;
            }
        }
    }
    if (truncated) result.output += "\n[linker output truncated]";
    result.code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}
#endif
[[maybe_unused]] bool static_image(const std::vector<std::uint8_t>& bytes) {
    using namespace ld_tk::detail;
    Ehdr header{};
    if (!read_at(bytes, 0, header) || std::memcmp(header.ident, "\177ELF", 4) ||
        header.ident[4] != ELFCLASS64 || header.ident[5] != ELFDATA2LSB || header.ident[6] != EV_CURRENT ||
        header.type != ET_EXEC || header.machine != EM_X86_64 || header.version != EV_CURRENT ||
        header.ehsize != sizeof(Ehdr) || header.phentsize != sizeof(Phdr) || !header.phnum ||
        !range_ok(bytes.size(), header.phoff, std::uint64_t(header.phnum) * sizeof(Phdr))) return false;
    bool entry = false;
    for (unsigned i = 0; i < header.phnum; ++i) {
        Phdr ph{};
        if (!read_at(bytes, header.phoff + i * sizeof(Phdr), ph)) return false;
        if (ph.type == 2 || ph.type == 3) return false; // PT_DYNAMIC / PT_INTERP
        if (ph.type == 0x6474e551 && (ph.flags & PF_X)) return false; // executable GNU_STACK
        if (ph.type == PT_LOAD) {
            if (!range_ok(bytes.size(), ph.offset, ph.filesz) || ph.filesz > ph.memsz ||
                ph.memsz > UINT64_MAX - ph.vaddr || !power_two(ph.align) ||
                (ph.align > 1 && ph.vaddr % ph.align != ph.offset % ph.align) ||
                (ph.flags & (PF_W | PF_X)) == (PF_W | PF_X)) return false;
            entry = entry || ((ph.flags & PF_X) && header.entry >= ph.vaddr && header.entry - ph.vaddr < ph.memsz);
        }
    }
    return entry;
}
} // namespace

bool native_runtime_object(const std::vector<std::uint8_t>& bytes) {
    ld_tk::detail::InputObject object;
    object.bytes = bytes;
    std::vector<ld_tk::Diagnostic> errors;
    if (!ld_tk::detail::parse_object(object, errors)) return false;
    bool entry = false, config = false, dispatch = false;
    for (const auto& symbol : object.symbols) {
        entry = entry || (symbol.name == "qs_native_entry" && symbol.s.shndx);
        config = config || (symbol.name == "qs_native_unit_data" && symbol.s.shndx);
        dispatch = dispatch || (symbol.name == "qs_native_dispatch" && !symbol.s.shndx);
    }
    return entry && config && dispatch;
}

Result<std::vector<std::uint8_t>> link_native_runtime(const std::vector<std::vector<std::uint8_t>>& objects, const Options& options) {
#if defined(__linux__)
    try {
        const auto& limits = options.toolchain.linker;
        const auto& native = options.native_link;
        const std::string driver = native.driver.empty() ? QSCRIPT_NATIVE_LINK_DRIVER : native.driver;
        const std::string archive = native.runtime_archive.empty() ? QSCRIPT_NATIVE_RUNTIME_ARCHIVE : native.runtime_archive;
        if (driver.empty() || archive.empty()) return failure("native grammar linking requires QSCRIPT_BUILD_NATIVE_RUNTIME or explicit native_link driver/runtime_archive paths");
        if (!native.timeout_seconds || native.timeout_seconds > 3600 || !native.max_diagnostic_bytes || native.max_diagnostic_bytes > 16u * 1024u * 1024u || native.arguments.size() > 256)
            return failure("invalid native linker resource limits");
        const auto valid_arg = [](const std::string& s) { return s.size() <= 65536 && s.find('\0') == std::string::npos; };
        if (!valid_arg(driver) || !valid_arg(archive) || !valid_arg(native.temporary_directory) ||
            std::any_of(native.arguments.begin(), native.arguments.end(), [&](const auto& arg) { return !valid_arg(arg); }))
            return failure("invalid native linker argument");
        std::error_code ec;
        if (!std::filesystem::is_regular_file(archive, ec)) return failure("native parser runtime archive is unavailable: " + archive);
        if (objects.empty() || objects.size() > limits.max_objects) return failure("input object count limit exceeded");
        std::uint64_t total = 0;
        for (const auto& bytes : objects) {
            if (bytes.size() > limits.max_input_size || bytes.size() > limits.max_total_input_size - std::min(total, limits.max_total_input_size))
                return failure("input size limit exceeded");
            total += bytes.size();
        }
        if (limits.allow_undefined || limits.entry_symbol != "_start" || limits.image_base % 4096)
            return failure("static native grammar linking requires defined symbols, CRT _start and a page-aligned image base");
        auto parent = native.temporary_directory.empty() ? std::filesystem::temp_directory_path() : std::filesystem::path(native.temporary_directory);
        auto pattern = (parent / "qscript-native-XXXXXX").string();
        std::vector<char> writable(pattern.begin(), pattern.end()); writable.push_back(0);
        if (!::mkdtemp(writable.data())) return failure("cannot create private native linker scratch directory");
        struct Scratch {
            const char* directory;
            std::vector<std::filesystem::path> files;
            ~Scratch() { std::error_code e; for (const auto& file : files) std::filesystem::remove(file, e); ::rmdir(directory); }
        } scratch{writable.data(), {}};
        std::vector<std::string> command{driver};
        command.insert(command.end(), native.arguments.begin(), native.arguments.end());
        command.insert(command.end(), {"-static", "-no-pie", "-Wl,--gc-sections", "-Wl,-z,noexecstack"});
        std::ostringstream base; base << "-Wl,-Ttext-segment=0x" << std::hex << limits.image_base;
        command.push_back(base.str());
        for (std::size_t i = 0; i < objects.size(); ++i) {
            auto file = std::filesystem::path(scratch.directory) / ("input" + std::to_string(i) + ".o"); scratch.files.push_back(file);
            std::ofstream out(file, std::ios::binary);
            out.write(reinterpret_cast<const char*>(objects[i].data()), static_cast<std::streamsize>(objects[i].size()));
            out.close();
            if (!out) return failure("writing native linker input failed");
            command.push_back(file.string());
        }
        command.push_back(archive);
        const auto image = std::filesystem::path(scratch.directory) / "program"; scratch.files.push_back(image);
        command.insert(command.end(), {"-o", image.string()});
        auto process = run_driver(std::move(command), native);
        if (!process.error.empty()) return failure(process.error + (process.output.empty() ? "" : "\n" + process.output));
        if (process.code != 0) return failure("static native grammar link failed (requires a C++ driver and static C++/C libraries)\n" + process.output);
        std::ifstream input(image, std::ios::binary);
        const auto limit = static_cast<std::size_t>(std::min<std::uint64_t>(limits.max_output_size, SIZE_MAX));
        auto contents = read_bounded(input, limit);
        if (!contents) return failure("native executable exceeds output size limit or cannot be read");
        std::vector<std::uint8_t> bytes(contents->begin(), contents->end());
        if (!static_image(bytes)) return failure("native linker did not produce a static x86-64 ELF executable with non-executable stack and W^X segments");
        return {std::move(bytes), {}};
    } catch (const std::exception& error) { return failure(error.what()); }
#else
    (void)objects; (void)options;
    return failure("static native grammar linking requires a Linux host toolchain");
#endif
}
} // namespace qscript::detail
