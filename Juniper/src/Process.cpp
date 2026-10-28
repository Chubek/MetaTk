#include "Internal.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <poll.h>
#include <set>
#include <spawn.h>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace juniper {
namespace {
struct FD {
    int value = -1;
    FD() = default;
    explicit FD(int v) : value(v) {}
    FD(const FD&) = delete;
    FD& operator=(const FD&) = delete;
    ~FD() { close(); }
    void close() { if (value >= 0) { ::close(value); value = -1; } }
};
struct Child {
    pid_t pid = -1;
    bool reaped = false, complete = false;
    ~Child() {
        if (pid > 0 && !complete) {
            ::kill(-pid, SIGKILL);
            if (!reaped) while (::waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
        }
    }
};
bool pipe_pair(FD& reader, FD& writer) {
    int fds[2];
    if (::pipe(fds) != 0) return false;
    reader.value = fds[0]; writer.value = fds[1];
    return ::fcntl(reader.value, F_SETFD, FD_CLOEXEC) != -1 &&
           ::fcntl(writer.value, F_SETFD, FD_CLOEXEC) != -1 &&
           ::fcntl(reader.value, F_SETFL, O_NONBLOCK) != -1;
}
std::filesystem::path executable_directory() {
    std::error_code ec;
    auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
    return ec ? std::filesystem::path{} : path.parent_path();
}
std::filesystem::path discover(const std::filesystem::path& explicit_path, const char* environment,
                               const std::vector<std::filesystem::path>& candidates) {
    if (!explicit_path.empty()) return explicit_path;
    if (const char* p = std::getenv(environment); p && *p) return p;
    for (const auto& p : candidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(p, ec)) return p;
    }
    return {};
}
Result<std::string> run(const std::vector<std::string>& arguments, const PreprocessOptions& options) {
    FD output, output_write, errors, errors_write;
    if (!pipe_pair(output, output_write) || !pipe_pair(errors, errors_write))
        return detail::error("JUN-E200", "cannot create GPPX pipes: " + std::string(std::strerror(errno)));
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    int rc = posix_spawn_file_actions_init(&actions);
    if (rc) return detail::error("JUN-E200", std::strerror(rc));
    rc = posix_spawnattr_init(&attributes);
    if (rc) { posix_spawn_file_actions_destroy(&actions); return detail::error("JUN-E200", std::strerror(rc)); }
    // All descriptors are close-on-exec except these explicit redirections.
    rc = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    if (!rc) rc = posix_spawn_file_actions_adddup2(&actions, output_write.value, STDOUT_FILENO);
    if (!rc) rc = posix_spawn_file_actions_adddup2(&actions, errors_write.value, STDERR_FILENO);
    if (!rc) rc = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    if (!rc) rc = posix_spawnattr_setpgroup(&attributes, 0);
    std::vector<char*> argv;
    for (const auto& arg : arguments) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    Child child;
    if (!rc) rc = posix_spawnp(&child.pid, argv.front(), &actions, &attributes, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attributes);
    if (rc) return detail::error("JUN-E200", "cannot start GPPX: " + std::string(std::strerror(rc)));
    output_write.close(); errors_write.close();
    std::string out, err;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(options.timeout_seconds);
    int status = 0;
    while (output.value >= 0 || errors.value >= 0 || !child.reaped) {
        if (std::chrono::steady_clock::now() >= deadline) return detail::error("JUN-E202", "GPPX expansion timed out");
        std::array<pollfd, 2> fds{{{output.value, POLLIN, 0}, {errors.value, POLLIN, 0}}};
        if (::poll(fds.data(), fds.size(), 50) < 0 && errno != EINTR)
            return detail::error("JUN-E200", "GPPX pipe polling failed");
        for (std::size_t i = 0; i < fds.size(); ++i) {
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            FD& fd = i == 0 ? output : errors;
            auto& text = i == 0 ? out : err;
            std::size_t limit = i == 0 ? options.max_output_bytes : 65536;
            std::array<char, 8192> buffer{};
            // One chunk per iteration ensures both pipes and the deadline get
            // serviced even if the child continuously produces output.
            auto count = ::read(fd.value, buffer.data(), buffer.size());
            if (count > 0) {
                if (static_cast<std::size_t>(count) > limit - text.size())
                    return detail::error("JUN-E203", "GPPX output limit exceeded");
                text.append(buffer.data(), static_cast<std::size_t>(count));
            } else if (count == 0) fd.close();
            else if (errno != EAGAIN && errno != EINTR) return detail::error("JUN-E200", "cannot read GPPX output");
        }
        if (!child.reaped) {
            auto result = ::waitpid(child.pid, &status, WNOHANG);
            if (result == child.pid) child.reaped = true;
            else if (result < 0 && errno != EINTR) return detail::error("JUN-E200", "cannot wait for GPPX");
        }
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return detail::error("JUN-E201", err.empty() ? "GPPX expansion failed" : err);
    // Warnings must not disappear silently when preprocessing is embedded.
    if (!err.empty()) return detail::error("JUN-E201", err);
    child.complete = true;
    return out;
}
} // namespace

Result<ExpandedSource> preprocess(const std::filesystem::path& input, const PreprocessOptions& options) {
    if (input.string().find('\0') != std::string::npos)
        return detail::error("JUN-E204", "NUL in input path");
    if (options.timeout_seconds == 0 || options.timeout_seconds > 3600 || options.max_output_bytes == 0 ||
        options.max_output_bytes > 64 * 1024 * 1024 || options.include_directories.size() > 120)
        return detail::error("JUN-E204", "invalid preprocessing limits");
    auto bin = executable_directory();
    auto executable = discover(options.executable, "JUNIPER_GPPX",
        {bin / "gppx", JUNIPER_BUILD_GPPX, JUNIPER_INSTALL_GPPX});
    auto library = discover(options.standard_library, "JUNIPER_STDLIB",
        {bin / "../share/juniper/stdlib/Juniper.xpp", JUNIPER_SOURCE_STDLIB, JUNIPER_INSTALL_STDLIB});
    if (executable.empty() || library.empty()) return detail::error("JUN-E200", "GPPX or Juniper.xpp not found; set JUNIPER_GPPX/JUNIPER_STDLIB");
    std::error_code ec;
    auto root = std::filesystem::canonical(input, ec);
    if (ec || !std::filesystem::is_regular_file(root)) return detail::error("JUN-E205", "cannot read input file", input.string());
    library = std::filesystem::absolute(library);
    // The profile is defined here, not inferred from file extensions or an
    // obsolete GPPX API. User macros are @name(...); meta directives remain #.
    std::vector<std::string> args{executable.string(), "-U", "@", "", "(", ",", ")", "(", ")", "#", "",
        "-M", "#", "\\n", " ", " ", "\\n", "(", ")", "-n",
        "+c", "/*", "*/", "+c", "//", "\\n", "+s", "\"", "\"", "\\",
        "--nostdinc", "--nocurinc", "--includemarker", "//__JUNIPER_SOURCE__ % % %",
        "--include", library.string()};
    std::vector<std::filesystem::path> directories{root.parent_path(), library.parent_path()};
    for (const auto& dir : options.include_directories) directories.push_back(std::filesystem::absolute(dir));
    for (const auto& dir : directories) args.push_back("-I" + dir.string());
    for (const auto& [name, value] : options.defines) {
        if (!detail::identifier(name) || value.find('\n') != std::string::npos || value.find('\r') != std::string::npos)
            return detail::error("JUN-E204", "invalid macro definition '" + name + "'");
        args.push_back("-D" + name + '=' + value);
    }
    args.push_back("--"); args.push_back(root.string());
    for (const auto& arg : args) if (arg.find('\0') != std::string::npos)
        return detail::error("JUN-E204", "NUL in GPPX argument");
    auto result = run(args, options);
    if (!result) { auto d = *result.error(); d.file = input.string(); return d; }
    ExpandedSource source; source.text = std::move(*result.get());
    std::set<std::filesystem::path> seen;
    auto dependency = [&](std::filesystem::path p) {
        if (p.is_relative()) {
            for (const auto& dir : directories) {
                std::error_code exists_error;
                if (std::filesystem::is_regular_file(dir / p, exists_error)) { p = dir / p; break; }
            }
        }
        std::error_code canonical_error;
        auto absolute = std::filesystem::canonical(p, canonical_error);
        if (!canonical_error && seen.insert(absolute).second) source.dependencies.push_back(std::move(absolute));
    };
    dependency(root); dependency(library);
    std::istringstream lines(source.text);
    std::string line;
    constexpr std::string_view marker = "//__JUNIPER_SOURCE__ ";
    while (std::getline(lines, line)) {
        if (!line.starts_with(marker)) continue;
        std::istringstream fields(line.substr(marker.size()));
        std::size_t number; std::string file;
        if (fields >> number >> std::quoted(file)) dependency(file);
    }
    return source;
}
} // namespace juniper
