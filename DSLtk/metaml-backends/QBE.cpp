// Out-of-process QBE compilation keeps QBE's global state and fatal diagnostics
// out of the embedding process. The generated code only calls the supplied ABI.
#include "../QBENative.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <dlfcn.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace {
struct Module {
  void *library = nullptr;
  std::size_t bytes = 0;
  std::vector<void *> entries;
  ~Module() { if (library) dlclose(library); }
};
struct Directory {
  std::filesystem::path path;
  Directory()
  {
    auto pattern = (std::filesystem::temp_directory_path() / "metaml-qbe-XXXXXX").string();
    if (!mkdtemp(pattern.data())) throw std::runtime_error("QBE: cannot create temporary directory");
    path = pattern;
  }
  ~Directory() { std::error_code error; std::filesystem::remove_all(path, error); }
};
std::string compiler()
{
  if (auto p = std::getenv("METAML_QBE")) return p;
#ifdef METAML_QBE_DEFAULT_COMPILER
  if (std::filesystem::exists(METAML_QBE_DEFAULT_COMPILER)) return METAML_QBE_DEFAULT_COMPILER;
#endif
  return "qbe";
}
std::string cc()
{
  if (auto p = std::getenv("METAML_QBE_CC")) return p;
#ifdef METAML_QBE_DEFAULT_CC
  return METAML_QBE_DEFAULT_CC;
#else
  return "cc";
#endif
}
const char *target()
{
#if defined(__x86_64__) && defined(__APPLE__)
  return "amd64_apple";
#elif defined(__x86_64__)
  return "amd64_sysv";
#elif defined(__aarch64__) && defined(__APPLE__)
  return "arm64_apple";
#elif defined(__aarch64__)
  return "arm64";
#elif defined(__riscv) && __riscv_xlen == 64
  return "rv64";
#else
  throw std::runtime_error("QBE: unsupported host architecture");
#endif
}
std::string diagnostics(const std::filesystem::path &path)
{
  std::ifstream input(path);
  std::string text(4096, '\0');
  input.read(text.data(), text.size());
  text.resize(static_cast<std::size_t>(input.gcount()));
  return text;
}
void run(std::vector<std::string> args, const std::filesystem::path &log)
{
  std::vector<char *> argv;
  for (auto &arg : args) argv.push_back(arg.data());
  argv.push_back(nullptr);
  posix_spawn_file_actions_t actions;
  int status = posix_spawn_file_actions_init(&actions);
  if (status) throw std::runtime_error("QBE: cannot initialize compiler process");
  struct Guard {
    posix_spawn_file_actions_t *actions;
    ~Guard() { posix_spawn_file_actions_destroy(actions); }
  } guard{&actions};
  status = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, log.c_str(),
                                           O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (!status) status = posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
  if (!status) status = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  if (status) throw std::runtime_error("QBE: cannot redirect compiler diagnostics");
  pid_t pid;
  status = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
  if (status) throw std::runtime_error("QBE: cannot execute " + args[0] + ": " + std::strerror(status));
  int result;
  pid_t waited;
  do { waited = waitpid(pid, &result, 0); } while (waited < 0 && errno == EINTR);
  if (waited < 0) throw std::runtime_error("QBE: cannot wait for compiler");
  if (!WIFEXITED(result) || WEXITSTATUS(result))
    throw std::runtime_error("QBE: compiler failed (" + args[0] + "): " + diagnostics(log));
}
void emit(std::ostream &out, const MetamlQbeCall *calls, const std::size_t *offsets, std::size_t count)
{
  for (std::size_t b = 0; b < count; ++b) {
    auto begin = offsets[b], end = offsets[b + 1];
    if (end <= begin || end - begin > 32) throw std::runtime_error("QBE: invalid basic block");
    out << "export function w $metaml_qbe_block_" << b << "(l %context) {\n@start\n";
    for (auto i = begin; i < end; ++i) {
      auto n = i - begin;
      out << "\t%r" << n << " =w call " << calls[i].function
          << "(l %context, l " << calls[i].data << ")\n";
      if (i + 1 < end)
        out << "\tjnz %r" << n << ", @exit" << n << ", @next" << n << "\n"
            << "@exit" << n << "\n\tret %r" << n << "\n@next" << n << "\n";
      else out << "\tret %r" << n << "\n";
    }
    out << "}\n";
  }
  if (!out) throw std::runtime_error("QBE: cannot write IL");
}
}

extern "C" int metaml_qbe_compile(const MetamlQbeCall *calls, const std::size_t *offsets,
  std::size_t count, std::size_t limit, void **module, char *error, std::size_t error_size) noexcept
{
  if (module) *module = nullptr;
  if (error && error_size) error[0] = 0;
  try {
#ifdef METAML_NO_JIT
    throw std::runtime_error("QBE: native execution disabled by METAML_NO_JIT");
#endif
    if (!module || !calls || !offsets || !count || offsets[0])
      throw std::runtime_error("QBE: invalid compilation request");
    Directory temp;
    auto il = temp.path / "module.ssa", assembly = temp.path / "module.s";
    auto library = temp.path / "module.so", log = temp.path / "compiler.log";
    { std::ofstream out(il); emit(out, calls, offsets, count); }
    run({compiler(), "-t", target(), "-o", assembly.string(), il.string()}, log);
#if defined(__APPLE__)
    run({cc(), "-dynamiclib", "-o", library.string(), assembly.string()}, log);
#else
    run({cc(), "-shared", "-Wl,-z,noexecstack", "-o", library.string(), assembly.string()}, log);
#endif
    auto compiled = std::make_unique<Module>();
    compiled->bytes = std::filesystem::file_size(library);
    if (compiled->bytes > limit) return 2;
    compiled->library = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!compiled->library) throw std::runtime_error(std::string("QBE: cannot load native module: ") + dlerror());
    for (std::size_t b = 0; b < count; ++b) {
      auto entry = dlsym(compiled->library, ("metaml_qbe_block_" + std::to_string(b)).c_str());
      if (!entry) throw std::runtime_error("QBE: missing compiled block entry");
      compiled->entries.push_back(entry);
    }
    *module = compiled.release();
    return 0;
  } catch (const std::exception &e) {
    if (error && error_size) {
      auto length = std::min(error_size - 1, std::strlen(e.what()));
      std::memcpy(error, e.what(), length); error[length] = 0;
    }
    return 1;
  } catch (...) { return 1; }
}
extern "C" void *metaml_qbe_entry(void *module, std::size_t block) noexcept
{
  auto m = static_cast<Module *>(module);
  return m && block < m->entries.size() ? m->entries[block] : nullptr;
}
extern "C" std::size_t metaml_qbe_size(void *module) noexcept
{
  return module ? static_cast<Module *>(module)->bytes : 0;
}
extern "C" void metaml_qbe_release(void *module) noexcept { delete static_cast<Module *>(module); }
