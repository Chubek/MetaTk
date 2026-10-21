#include "dynload.h"

#include <qBFD.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#  define NOMINMAX
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <unistd.h>
#  if defined(__APPLE__)
#    include <mach-o/dyld.h>
#  endif
#endif

namespace {

thread_local std::string g_error;

tdyn_status_t fail(tdyn_status_t status, std::string message) {
  g_error = std::move(message);
  return status;
}

tdyn_status_t ok() {
  g_error.clear();
  return TDYN_OK;
}

const char* empty() { return ""; }

bool hasPathSeparator(std::string_view s) {
  return s.find('/') != std::string_view::npos ||
         s.find('\\') != std::string_view::npos;
}

bool existsRegular(const std::filesystem::path& path) {
  std::error_code ec;
  return std::filesystem::is_regular_file(path, ec);
}

std::filesystem::path absoluteIfPossible(const std::filesystem::path& path) {
  std::error_code ec;
  auto out = std::filesystem::weakly_canonical(path, ec);
  if (!ec) return out;
  out = std::filesystem::absolute(path, ec);
  return ec ? path : out;
}

char pathListSeparator() {
#if defined(_WIN32)
  return ';';
#else
  return ':';
#endif
}

std::vector<std::string> splitPathList(const char* value) {
  std::vector<std::string> out;
  if (!value || !*value) return out;
  std::string text(value);
  char sep = pathListSeparator();
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find(sep, start);
    std::string item = text.substr(start, end == std::string::npos
                                           ? std::string::npos
                                           : end - start);
    if (!item.empty()) out.push_back(std::move(item));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return out;
}

std::vector<std::string> candidateNames(std::string_view name) {
  std::vector<std::string> out;
  std::string n(name);
  out.push_back(n);
  std::filesystem::path p(n);
  bool hasExtension = p.has_extension();
  auto add = [&](std::string value) {
    if (std::find(out.begin(), out.end(), value) == out.end())
      out.push_back(std::move(value));
  };
#if defined(_WIN32)
  if (!hasExtension) {
    add(n + ".dll");
    add("lib" + n + ".dll");
  }
#elif defined(__APPLE__)
  if (!hasExtension) {
    add("lib" + n + ".dylib");
    add(n + ".dylib");
    add("lib" + n + ".so");
    add(n + ".so");
  }
#else
  if (!hasExtension) {
    add("lib" + n + ".so");
    add(n + ".so");
  }
#endif
  return out;
}

#if defined(_WIN32)
using NativeHandle = HMODULE;

std::string windowsError(DWORD code = GetLastError()) {
  if (code == 0) return "Windows loader error";
  char* buffer = nullptr;
  DWORD n = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                               FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr, code, 0, reinterpret_cast<LPSTR>(&buffer),
                           0, nullptr);
  std::string out = n && buffer ? std::string(buffer, n)
                                : "Windows error " + std::to_string(code);
  if (buffer) LocalFree(buffer);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
  return out;
}

tdyn_status_t nativeOpen(const char* path, uint32_t flags, NativeHandle* out,
                         bool* owned) {
  if (!out || !owned) return fail(TDYN_INVALID_ARGUMENT, "missing native output");
  *out = nullptr;
  *owned = false;
  if (flags & TDYN_OPEN_DEEPBIND)
    return fail(TDYN_UNSUPPORTED, "TDYN_OPEN_DEEPBIND is not supported on Windows");
  if (path == nullptr) {
    HMODULE self = GetModuleHandleA(nullptr);
    if (!self) return fail(TDYN_NATIVE_ERROR, windowsError());
    *out = self;
    return ok();
  }
  if (flags & TDYN_OPEN_NOLOAD) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(0, path, &module))
      return fail(TDYN_NOT_FOUND, windowsError());
    *out = module;
    *owned = true;
    return ok();
  }
  HMODULE module = LoadLibraryExA(path, nullptr, 0);
  if (!module) return fail(TDYN_NATIVE_ERROR, windowsError());
  if (flags & TDYN_OPEN_NODELETE) {
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN, path, &pinned)) {
      auto message = windowsError();
      FreeLibrary(module);
      return fail(TDYN_NATIVE_ERROR, std::move(message));
    }
  }
  *out = module;
  // Pinning controls unmapping, not ownership of this open's reference.
  *owned = true;
  return ok();
}

void nativeClose(NativeHandle handle, bool owned) {
  if (handle && owned) FreeLibrary(handle);
}

bool nativeSymbol(NativeHandle handle, const char* name, void** out,
                  std::string* error) {
  if (!handle || !name || !out) {
    if (error) *error = "invalid native symbol lookup";
    return false;
  }
  FARPROC proc = GetProcAddress(handle, name);
  if (!proc) {
    if (error) *error = windowsError();
    return false;
  }
  *out = reinterpret_cast<void*>(proc);
  return true;
}

std::filesystem::path currentExecutablePath() {
  std::vector<char> buffer(32768);
  DWORD n = GetModuleFileNameA(nullptr, buffer.data(), DWORD(buffer.size()));
  if (n == 0 || n >= buffer.size()) return {};
  return std::filesystem::path(std::string(buffer.data(), n));
}

#else
using NativeHandle = void*;

tdyn_status_t nativeOpen(const char* path, uint32_t flags, NativeHandle* out,
                         bool* owned) {
  if (!out || !owned) return fail(TDYN_INVALID_ARGUMENT, "missing native output");
  *out = nullptr;
  *owned = false;
  int mode = 0;
  if ((flags & TDYN_OPEN_LAZY) && !(flags & TDYN_OPEN_NOW))
    mode |= RTLD_LAZY;
  else
    mode |= RTLD_NOW;
  mode |= (flags & TDYN_OPEN_GLOBAL) ? RTLD_GLOBAL : RTLD_LOCAL;
#ifdef RTLD_NODELETE
  if (flags & TDYN_OPEN_NODELETE) mode |= RTLD_NODELETE;
#else
  if (flags & TDYN_OPEN_NODELETE) {
    return fail(TDYN_UNSUPPORTED,
                "TDYN_OPEN_NODELETE is not supported by this loader");
  }
#endif
#ifdef RTLD_NOLOAD
  if (flags & TDYN_OPEN_NOLOAD) mode |= RTLD_NOLOAD;
#else
  if (flags & TDYN_OPEN_NOLOAD)
    return fail(TDYN_UNSUPPORTED,
                "TDYN_OPEN_NOLOAD is not supported by this loader");
#endif
#ifdef RTLD_DEEPBIND
  if (flags & TDYN_OPEN_DEEPBIND) mode |= RTLD_DEEPBIND;
#else
  if (flags & TDYN_OPEN_DEEPBIND)
    return fail(TDYN_UNSUPPORTED,
                "TDYN_OPEN_DEEPBIND is not supported by this loader");
#endif
  dlerror();
  void* handle = dlopen(path, mode);
  if (!handle) {
    const char* err = dlerror();
    return fail((flags & TDYN_OPEN_NOLOAD) ? TDYN_NOT_FOUND : TDYN_NATIVE_ERROR,
                err ? err : "dlopen failed");
  }
  *out = handle;
  *owned = true;
  return ok();
}

void nativeClose(NativeHandle handle, bool owned) {
  if (handle && owned) dlclose(handle);
}

bool nativeSymbol(NativeHandle handle, const char* name, void** out,
                  std::string* error) {
  if (!handle || !name || !out) {
    if (error) *error = "invalid native symbol lookup";
    return false;
  }
  dlerror();
  void* symbol = dlsym(handle, name);
  const char* err = dlerror();
  if (err) {
    if (error) *error = err;
    return false;
  }
  *out = symbol;
  return true;
}

std::filesystem::path currentExecutablePath() {
#if defined(__linux__)
  std::vector<char> buffer(4096);
  for (;;) {
    ssize_t n = readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (n < 0) return {};
    if (static_cast<size_t>(n) < buffer.size())
      return std::filesystem::path(std::string(buffer.data(), size_t(n)));
    buffer.resize(buffer.size() * 2);
  }
#elif defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> buffer(size + 1);
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
  return std::filesystem::path(buffer.data());
#else
  return {};
#endif
}

#endif

}  // namespace

struct Native {
  NativeHandle handle = nullptr;
  bool owned = false;
};

struct tdyn_library {
  ~tdyn_library() { nativeClose(native.handle, native.owned); }
  std::filesystem::path path;
  std::string label;
  uint32_t flags = 0;
  tdyn_env* owner = nullptr;
  Native native;
  std::string inspectError;
  std::shared_ptr<qbfd::Buffer> buffer;
  std::unique_ptr<qbfd::ObjectFile> object;
  mutable std::vector<std::string> needed;
  mutable bool neededReady = false;
  mutable std::vector<qbfd::Relocation> relocations;
  mutable bool relocationsReady = false;
  mutable std::string relocationError;
};

struct EnvSymbol {
  void* address = nullptr;
  tdyn_library* owner = nullptr;
};

struct tdyn_env {
  std::vector<std::string> searchPaths;
  std::vector<std::unique_ptr<tdyn_library>> libraries;
  std::unordered_map<std::string, EnvSymbol> symbols;
};

namespace {

void closeLibrary(tdyn_library* library) {
  if (!library) return;
  nativeClose(library->native.handle, library->native.owned);
  library->native.handle = nullptr;
  library->native.owned = false;
}

void inspectLibrary(tdyn_library& library) {
  library.inspectError.clear();
  if (library.path.empty()) {
    library.inspectError = "no file path available for qBFD inspection";
    return;
  }
  auto opened = qbfd::open(library.path);
  if (!opened) {
    library.inspectError = opened.error().message;
    return;
  }
  library.buffer = std::move(opened->buffer);
  library.object = std::move(opened->object);
}

std::optional<std::filesystem::path> resolveLibrary(tdyn_env* env,
                                                    std::string_view name) {
  if (name.empty()) return std::nullopt;
  std::filesystem::path direct(name);
  if (hasPathSeparator(name) || direct.is_absolute()) {
    if (existsRegular(direct)) return absoluteIfPossible(direct);
    return std::nullopt;
  }
  if (existsRegular(direct)) return absoluteIfPossible(direct);
  if (!env) return std::nullopt;
  for (const auto& dir : env->searchPaths) {
    for (const auto& candidate : candidateNames(name)) {
      auto path = std::filesystem::path(dir) / candidate;
      if (existsRegular(path)) return absoluteIfPossible(path);
    }
  }
  return std::nullopt;
}

std::unique_ptr<tdyn_library> makeLibrary(const char* path, uint32_t flags,
                                          bool self, tdyn_env* env,
                                          tdyn_status_t* status) {
  if (status) *status = TDYN_ERROR;
  constexpr uint32_t known = TDYN_OPEN_LAZY | TDYN_OPEN_NOW | TDYN_OPEN_LOCAL |
      TDYN_OPEN_GLOBAL | TDYN_OPEN_NODELETE | TDYN_OPEN_NOLOAD | TDYN_OPEN_DEEPBIND |
      TDYN_OPEN_REQUIRE_INSPECT | TDYN_OPEN_INSPECT_ONLY;
  if ((flags & ~known) ||
      ((flags & TDYN_OPEN_LAZY) && (flags & TDYN_OPEN_NOW)) ||
      ((flags & TDYN_OPEN_LOCAL) && (flags & TDYN_OPEN_GLOBAL))) {
    if (status) *status = fail(TDYN_INVALID_ARGUMENT, "invalid or conflicting open flags");
    return nullptr;
  }
  if (!self && (!path || !*path)) {
    if (status) *status = fail(TDYN_INVALID_ARGUMENT, "library path is required");
    return nullptr;
  }
  if ((flags & TDYN_OPEN_INSPECT_ONLY) && self) {
    if (status) *status = fail(TDYN_INVALID_ARGUMENT,
                               "self handle cannot be inspect-only");
    return nullptr;
  }

  std::filesystem::path inspectPath;
  std::string nativePath;
  if (self) {
    inspectPath = currentExecutablePath();
  } else {
    std::string_view requested(path);
    if (auto resolved = resolveLibrary(env, requested)) {
      inspectPath = *resolved;
      nativePath = resolved->string();
    } else {
      nativePath = std::string(requested);
      std::filesystem::path candidate(requested);
      if (existsRegular(candidate)) inspectPath = absoluteIfPossible(candidate);
    }
  }

  auto library = std::make_unique<tdyn_library>();
  if (!(flags & TDYN_OPEN_INSPECT_ONLY)) {
    tdyn_status_t rc = nativeOpen(self ? nullptr : nativePath.c_str(), flags,
                                  &library->native.handle,
                                  &library->native.owned);
    if (rc != TDYN_OK) {
      if (status) *status = rc;
      return nullptr;
    }
  }
  library->path = inspectPath;
  library->label = self ? std::string("<self>")
                        : (inspectPath.empty() ? nativePath : inspectPath.string());
  library->flags = flags;
  inspectLibrary(*library);
  if ((flags & (TDYN_OPEN_REQUIRE_INSPECT | TDYN_OPEN_INSPECT_ONLY)) && !library->object) {
    std::string message = library->inspectError.empty()
                              ? "qBFD inspection failed"
                              : library->inspectError;
    closeLibrary(library.get());
    if (status) *status = fail(TDYN_INSPECT_ERROR, std::move(message));
    return nullptr;
  }
  if (status) *status = ok();
  return library;
}

tdyn_symbol_binding_t mapBinding(qbfd::SymbolBinding binding) {
  switch (binding) {
    case qbfd::SymbolBinding::Global: return TDYN_SYMBOL_GLOBAL;
    case qbfd::SymbolBinding::Weak: return TDYN_SYMBOL_WEAK;
    case qbfd::SymbolBinding::Unique: return TDYN_SYMBOL_UNIQUE;
    case qbfd::SymbolBinding::Local:
    default: return TDYN_SYMBOL_LOCAL;
  }
}

tdyn_symbol_kind_t mapKind(qbfd::SymbolKind kind) {
  switch (kind) {
    case qbfd::SymbolKind::Object: return TDYN_SYMBOL_OBJECT;
    case qbfd::SymbolKind::Function: return TDYN_SYMBOL_FUNCTION;
    case qbfd::SymbolKind::Section: return TDYN_SYMBOL_SECTION;
    case qbfd::SymbolKind::File: return TDYN_SYMBOL_FILE;
    case qbfd::SymbolKind::Common: return TDYN_SYMBOL_COMMON;
    case qbfd::SymbolKind::Tls: return TDYN_SYMBOL_TLS;
    case qbfd::SymbolKind::Indirect: return TDYN_SYMBOL_INDIRECT;
    case qbfd::SymbolKind::None:
    default: return TDYN_SYMBOL_NONE;
  }
}

void fillSymbolInfo(const qbfd::Symbol& symbol, tdyn_symbol_table_t table,
                    tdyn_symbol_info_t* out) {
  std::memset(out, 0, sizeof(*out));
  out->name = symbol.name.c_str();
  out->value = symbol.value;
  out->size = symbol.size;
  out->flags = symbol.flags;
  out->native_type = symbol.nativeType;
  out->binding = mapBinding(symbol.binding);
  out->kind = mapKind(symbol.kind);
  out->table = table;
  out->undefined = (symbol.flags & qbfd::sym::Undefined) != 0;
  out->exported = (symbol.flags & qbfd::sym::Exported) != 0;
  out->imported = (symbol.flags & qbfd::sym::Imported) != 0;
  out->hidden = (symbol.flags & qbfd::sym::Hidden) != 0;
  if (symbol.section) {
    out->has_section = 1;
    out->section = *symbol.section;
  }
}

const std::vector<qbfd::Symbol>* symbolTable(const tdyn_library* library,
                                             tdyn_symbol_table_t table) {
  if (!library || !library->object) return nullptr;
  if (table == TDYN_SYMBOL_TABLE_STATIC) return &library->object->symbols();
  if (table == TDYN_SYMBOL_TABLE_DYNAMIC) return &library->object->dynamicSymbols();
  return nullptr;
}

tdyn_status_t ensureRelocations(const tdyn_library* library) {
  if (!library || !library->object)
    return fail(TDYN_INSPECT_ERROR, "library has no qBFD inspection data");
  if (library->relocationsReady) return TDYN_OK;
  auto relocs = library->object->allRelocations();
  if (!relocs) {
    library->relocationError = relocs.error().message;
    return fail(TDYN_INSPECT_ERROR, library->relocationError);
  }
  library->relocations = std::move(*relocs);
  library->relocationsReady = true;
  return TDYN_OK;
}

tdyn_status_t copyResolvedPath(tdyn_env* env, const char* name, char* out,
                               size_t* inout_size) {
  if (!env || !name || !inout_size)
    return fail(TDYN_INVALID_ARGUMENT, "environment, name and size are required");
  auto resolved = resolveLibrary(env, name);
  if (!resolved) return fail(TDYN_NOT_FOUND, "library not found: " + std::string(name));
  std::string value = resolved->string();
  size_t required = value.size() + 1;
  if (!out || *inout_size < required) {
    *inout_size = required;
    return fail(TDYN_BUFFER_TOO_SMALL, "output buffer is too small");
  }
  std::memcpy(out, value.c_str(), required);
  *inout_size = required;
  return ok();
}

tdyn_status_t addDefaultPaths(tdyn_env* env) {
  if (!env) return fail(TDYN_INVALID_ARGUMENT, "environment is required");
  auto addAll = [&](const char* value) {
    for (auto& p : splitPathList(value)) env->searchPaths.emplace_back(std::move(p));
  };
#if defined(_WIN32)
  addAll(std::getenv("PATH"));
#elif defined(__APPLE__)
  addAll(std::getenv("DYLD_LIBRARY_PATH"));
  addAll(std::getenv("LD_LIBRARY_PATH"));
#else
  addAll(std::getenv("LD_LIBRARY_PATH"));
#endif
  return ok();
}

}  // namespace

extern "C" {

const char* tdyn_version(void) { return "0.1.0"; }

const char* tdyn_status_string(tdyn_status_t status) {
  switch (status) {
    case TDYN_OK: return "ok";
    case TDYN_ERROR: return "error";
    case TDYN_INVALID_ARGUMENT: return "invalid argument";
    case TDYN_NOT_FOUND: return "not found";
    case TDYN_NATIVE_ERROR: return "native loader error";
    case TDYN_INSPECT_ERROR: return "object inspection error";
    case TDYN_UNSUPPORTED: return "unsupported";
    case TDYN_OUT_OF_MEMORY: return "out of memory";
    case TDYN_BUFFER_TOO_SMALL: return "buffer too small";
    default: return "unknown status";
  }
}

const char* tdyn_last_error(void) { return g_error.c_str(); }

const char* tdyn_symbol_binding_name(tdyn_symbol_binding_t binding) {
  switch (binding) {
    case TDYN_SYMBOL_LOCAL: return "local";
    case TDYN_SYMBOL_GLOBAL: return "global";
    case TDYN_SYMBOL_WEAK: return "weak";
    case TDYN_SYMBOL_UNIQUE: return "unique";
    default: return "unknown";
  }
}

const char* tdyn_symbol_kind_name(tdyn_symbol_kind_t kind) {
  switch (kind) {
    case TDYN_SYMBOL_NONE: return "none";
    case TDYN_SYMBOL_OBJECT: return "object";
    case TDYN_SYMBOL_FUNCTION: return "function";
    case TDYN_SYMBOL_SECTION: return "section";
    case TDYN_SYMBOL_FILE: return "file";
    case TDYN_SYMBOL_COMMON: return "common";
    case TDYN_SYMBOL_TLS: return "tls";
    case TDYN_SYMBOL_INDIRECT: return "indirect";
    default: return "unknown";
  }
}

tdyn_status_t tdyn_env_create(tdyn_env_t** out) {
  if (!out) return fail(TDYN_INVALID_ARGUMENT, "output environment is required");
  try {
    *out = new tdyn_env;
    return ok();
  } catch (const std::bad_alloc&) {
    *out = nullptr;
    return fail(TDYN_OUT_OF_MEMORY, "cannot allocate dynload environment");
  }
}

tdyn_env_t* tdyn_env_new(void) {
  tdyn_env_t* result = nullptr;
  (void)tdyn_env_create(&result);
  return result;
}

void tdyn_env_destroy(tdyn_env_t* env) { delete env; }

tdyn_status_t tdyn_env_add_search_path(tdyn_env_t* env, const char* path) {
  if (!env || !path || !*path)
    return fail(TDYN_INVALID_ARGUMENT, "environment and path are required");
  try {
    env->searchPaths.emplace_back(path);
    return ok();
  } catch (const std::bad_alloc&) {
    return fail(TDYN_OUT_OF_MEMORY, "cannot add search path");
  }
}

tdyn_status_t tdyn_env_add_default_search_paths(tdyn_env_t* env) {
  try {
    return addDefaultPaths(env);
  } catch (const std::bad_alloc&) {
    return fail(TDYN_OUT_OF_MEMORY, "cannot add default search paths");
  }
}

size_t tdyn_env_search_path_count(const tdyn_env_t* env) {
  return env ? env->searchPaths.size() : 0;
}

const char* tdyn_env_search_path(const tdyn_env_t* env, size_t index) {
  if (!env || index >= env->searchPaths.size()) return nullptr;
  return env->searchPaths[index].c_str();
}

tdyn_status_t tdyn_env_resolve_library(tdyn_env_t* env, const char* name,
                                       char* out, size_t* inout_size) {
  try {
    return copyResolvedPath(env, name, out, inout_size);
  } catch (const std::bad_alloc&) {
    return fail(TDYN_OUT_OF_MEMORY, "cannot resolve library");
  }
}

tdyn_status_t tdyn_library_open(const char* path, uint32_t flags,
                                tdyn_library_t** out) {
  if (!out) return fail(TDYN_INVALID_ARGUMENT, "output library is required");
  *out = nullptr;
  try {
    tdyn_status_t status = TDYN_ERROR;
    auto library = makeLibrary(path, flags, false, nullptr, &status);
    if (!library) return status;
    *out = library.release();
    return ok();
  } catch (const std::bad_alloc&) {
    return fail(TDYN_OUT_OF_MEMORY, "cannot allocate library handle");
  } catch (const std::exception& e) {
    return fail(TDYN_ERROR, e.what());
  }
}

tdyn_library_t* tdyn_library_load(const char* path, uint32_t flags) {
  tdyn_library_t* result = nullptr;
  (void)tdyn_library_open(path, flags, &result);
  return result;
}

tdyn_status_t tdyn_library_open_self(uint32_t flags, tdyn_library_t** out) {
  if (!out) return fail(TDYN_INVALID_ARGUMENT, "output library is required");
  *out = nullptr;
  try {
    tdyn_status_t status = TDYN_ERROR;
    auto library = makeLibrary(nullptr, flags, true, nullptr, &status);
    if (!library) return status;
    *out = library.release();
    return ok();
  } catch (const std::bad_alloc&) {
    return fail(TDYN_OUT_OF_MEMORY, "cannot allocate self handle");
  } catch (const std::exception& e) {
    return fail(TDYN_ERROR, e.what());
  }
}

void tdyn_library_close(tdyn_library_t* library) {
  if (!library) return;
  if (auto* env = library->owner) {
    for (auto it = env->symbols.begin(); it != env->symbols.end();) {
      if (it->second.owner == library) it = env->symbols.erase(it);
      else ++it;
    }
    auto it = std::find_if(env->libraries.begin(), env->libraries.end(),
                          [library](const auto& entry) { return entry.get() == library; });
    if (it != env->libraries.end()) env->libraries.erase(it);
    return;
  }
  closeLibrary(library);
  delete library;
}

tdyn_status_t tdyn_env_open(tdyn_env_t* env, const char* path, uint32_t flags,
                            tdyn_library_t** out) {
  if (out) *out = nullptr;
  if (!env || !out)
    return fail(TDYN_INVALID_ARGUMENT, "environment and output are required");
  try {
    tdyn_status_t status = TDYN_ERROR;
    auto library = makeLibrary(path, flags, false, env, &status);
    if (!library) return status;
    tdyn_library_t* raw = library.get();
    raw->owner = env;
    env->libraries.push_back(std::move(library));
    *out = raw;
    return ok();
  } catch (const std::bad_alloc&) {
    return fail(TDYN_OUT_OF_MEMORY, "cannot add library to environment");
  } catch (const std::exception& e) {
    return fail(TDYN_ERROR, e.what());
  }
}

tdyn_status_t tdyn_env_open_self(tdyn_env_t* env, uint32_t flags,
                                 tdyn_library_t** out) {
  if (out) *out = nullptr;
  if (!env || !out)
    return fail(TDYN_INVALID_ARGUMENT, "environment and output are required");
  try {
    tdyn_status_t status = TDYN_ERROR;
    auto library = makeLibrary(nullptr, flags, true, env, &status);
    if (!library) return status;
    tdyn_library_t* raw = library.get();
    raw->owner = env;
    env->libraries.push_back(std::move(library));
    *out = raw;
    return ok();
  } catch (const std::bad_alloc&) {
    return fail(TDYN_OUT_OF_MEMORY, "cannot add self handle to environment");
  } catch (const std::exception& e) {
    return fail(TDYN_ERROR, e.what());
  }
}

size_t tdyn_env_library_count(const tdyn_env_t* env) {
  return env ? env->libraries.size() : 0;
}

tdyn_library_t* tdyn_env_library(tdyn_env_t* env, size_t index) {
  if (!env || index >= env->libraries.size()) return nullptr;
  return env->libraries[index].get();
}

const char* tdyn_library_path(const tdyn_library_t* library) {
  if (!library) return nullptr;
  return library->label.c_str();
}

int tdyn_library_is_native_loaded(const tdyn_library_t* library) {
  return library && library->native.handle ? 1 : 0;
}

int tdyn_library_is_inspected(const tdyn_library_t* library) {
  return library && library->object ? 1 : 0;
}

const char* tdyn_library_inspection_error(const tdyn_library_t* library) {
  if (!library) return "invalid library";
  return library->inspectError.c_str();
}

const char* tdyn_library_format_name(const tdyn_library_t* library) {
  if (!library || !library->object) return empty();
  return qbfd::toString(library->object->format()).data();
}

const char* tdyn_library_arch_name(const tdyn_library_t* library) {
  if (!library || !library->object) return empty();
  return qbfd::toString(library->object->arch()).data();
}

const char* tdyn_library_endian_name(const tdyn_library_t* library) {
  if (!library || !library->object) return empty();
  return qbfd::toString(library->object->endian()).data();
}

const char* tdyn_library_file_type_name(const tdyn_library_t* library) {
  if (!library || !library->object) return empty();
  return qbfd::toString(library->object->fileType()).data();
}

uint64_t tdyn_library_entry_point(const tdyn_library_t* library) {
  return library && library->object ? library->object->entryPoint() : 0;
}

uint64_t tdyn_library_image_base(const tdyn_library_t* library) {
  return library && library->object ? library->object->imageBase() : 0;
}

tdyn_status_t tdyn_library_symbol(tdyn_library_t* library, const char* name,
                                  void** out) {
  if (out) *out = nullptr;
  if (!library || !name || !out)
    return fail(TDYN_INVALID_ARGUMENT, "library, symbol name and output are required");
  if (!library->native.handle)
    return fail(TDYN_UNSUPPORTED, "library is not loaded by the native loader");
  std::string error;
  if (!nativeSymbol(library->native.handle, name, out, &error))
    return fail(TDYN_NOT_FOUND, error.empty() ? "symbol not found" : error);
  return ok();
}

tdyn_status_t tdyn_library_find_symbol_info(const tdyn_library_t* library,
                                            const char* name,
                                            tdyn_symbol_table_t table,
                                            tdyn_symbol_info_t* out) {
  if (!library || !name || !out)
    return fail(TDYN_INVALID_ARGUMENT, "library, symbol name and output are required");
  if (!library->object)
    return fail(TDYN_INSPECT_ERROR, "library has no qBFD inspection data");
  auto scan = [&](tdyn_symbol_table_t which) -> const qbfd::Symbol* {
    const auto* symbols = symbolTable(library, which);
    if (!symbols) return nullptr;
    for (const auto& symbol : *symbols)
      if (symbol.name == name) return &symbol;
    return nullptr;
  };
  if (table == TDYN_SYMBOL_TABLE_ANY) {
    if (const qbfd::Symbol* s = scan(TDYN_SYMBOL_TABLE_STATIC)) {
      fillSymbolInfo(*s, TDYN_SYMBOL_TABLE_STATIC, out);
      return ok();
    }
    if (const qbfd::Symbol* s = scan(TDYN_SYMBOL_TABLE_DYNAMIC)) {
      fillSymbolInfo(*s, TDYN_SYMBOL_TABLE_DYNAMIC, out);
      return ok();
    }
  } else if (const qbfd::Symbol* s = scan(table)) {
    fillSymbolInfo(*s, table, out);
    return ok();
  }
  return fail(TDYN_NOT_FOUND, "symbol metadata not found: " + std::string(name));
}

const tdyn_symbol_info_t* tdyn_library_find_symbol_info_ptr(
    const tdyn_library_t* library, const char* name, tdyn_symbol_table_t table) {
  thread_local tdyn_symbol_info_t info;
  return tdyn_library_find_symbol_info(library, name, table, &info) == TDYN_OK
             ? &info
             : nullptr;
}

size_t tdyn_library_symbol_count(const tdyn_library_t* library,
                                 tdyn_symbol_table_t table) {
  if (!library || !library->object) return 0;
  if (table == TDYN_SYMBOL_TABLE_ANY)
    return library->object->symbols().size() +
           library->object->dynamicSymbols().size();
  const auto* symbols = symbolTable(library, table);
  return symbols ? symbols->size() : 0;
}

tdyn_status_t tdyn_library_symbol_info(const tdyn_library_t* library,
                                       tdyn_symbol_table_t table,
                                       size_t index,
                                       tdyn_symbol_info_t* out) {
  if (!library || !out)
    return fail(TDYN_INVALID_ARGUMENT, "library and output are required");
  if (!library->object)
    return fail(TDYN_INSPECT_ERROR, "library has no qBFD inspection data");
  if (table == TDYN_SYMBOL_TABLE_ANY) {
    const auto& statics = library->object->symbols();
    if (index < statics.size()) {
      fillSymbolInfo(statics[index], TDYN_SYMBOL_TABLE_STATIC, out);
      return ok();
    }
    index -= statics.size();
    const auto& dynamics = library->object->dynamicSymbols();
    if (index < dynamics.size()) {
      fillSymbolInfo(dynamics[index], TDYN_SYMBOL_TABLE_DYNAMIC, out);
      return ok();
    }
    return fail(TDYN_NOT_FOUND, "symbol index out of range");
  }
  const auto* symbols = symbolTable(library, table);
  if (!symbols) return fail(TDYN_INVALID_ARGUMENT, "invalid symbol table");
  if (index >= symbols->size())
    return fail(TDYN_NOT_FOUND, "symbol index out of range");
  fillSymbolInfo((*symbols)[index], table, out);
  return ok();
}

tdyn_status_t tdyn_library_symbol_at(const tdyn_library_t* library,
                                     uint64_t address,
                                     tdyn_symbol_table_t table,
                                     tdyn_symbol_info_t* out) {
  if (!library || !out)
    return fail(TDYN_INVALID_ARGUMENT, "library and output are required");
  if (!library->object)
    return fail(TDYN_INSPECT_ERROR, "library has no qBFD inspection data");
  const qbfd::Symbol* symbol = nullptr;
  tdyn_symbol_table_t actual = table;
  if (table == TDYN_SYMBOL_TABLE_ANY || table == TDYN_SYMBOL_TABLE_STATIC) {
    symbol = library->object->symbolAtAddress(address, false);
    actual = TDYN_SYMBOL_TABLE_STATIC;
  }
  if (!symbol && (table == TDYN_SYMBOL_TABLE_ANY || table == TDYN_SYMBOL_TABLE_DYNAMIC)) {
    symbol = library->object->symbolAtAddress(address, true);
    actual = TDYN_SYMBOL_TABLE_DYNAMIC;
  }
  if (!symbol) return fail(TDYN_NOT_FOUND, "no symbol covers address");
  fillSymbolInfo(*symbol, actual, out);
  return ok();
}

size_t tdyn_library_needed_count(const tdyn_library_t* library) {
  if (!library || !library->object) return 0;
  if (!library->neededReady) {
    library->needed = library->object->neededLibraries();
    library->neededReady = true;
  }
  return library->needed.size();
}

const char* tdyn_library_needed(const tdyn_library_t* library, size_t index) {
  if (!library) return nullptr;
  (void)tdyn_library_needed_count(library);
  if (index >= library->needed.size()) return nullptr;
  return library->needed[index].c_str();
}

size_t tdyn_library_section_count(const tdyn_library_t* library) {
  return library && library->object ? library->object->sections().size() : 0;
}

tdyn_status_t tdyn_library_section_info(const tdyn_library_t* library,
                                        size_t index,
                                        tdyn_section_info_t* out) {
  if (!library || !out)
    return fail(TDYN_INVALID_ARGUMENT, "library and output are required");
  if (!library->object)
    return fail(TDYN_INSPECT_ERROR, "library has no qBFD inspection data");
  const auto& sections = library->object->sections();
  if (index >= sections.size()) return fail(TDYN_NOT_FOUND, "section index out of range");
  const auto& s = sections[index];
  std::memset(out, 0, sizeof(*out));
  out->name = s.name.c_str();
  out->index = s.index;
  out->vma = s.vma;
  out->lma = s.lma;
  out->size = s.size;
  out->file_offset = s.fileOffset;
  out->file_size = s.fileSize;
  out->alignment = s.alignment;
  out->flags = s.flags;
  out->native_flags = s.nativeFlags;
  out->native_type = s.nativeType;
  return ok();
}

size_t tdyn_library_relocation_count(const tdyn_library_t* library) {
  if (!library || ensureRelocations(library) != TDYN_OK) return 0;
  return library->relocations.size();
}

tdyn_status_t tdyn_library_relocation_info(const tdyn_library_t* library,
                                           size_t index,
                                           tdyn_relocation_info_t* out) {
  if (!library || !out)
    return fail(TDYN_INVALID_ARGUMENT, "library and output are required");
  tdyn_status_t rc = ensureRelocations(library);
  if (rc != TDYN_OK) return rc;
  if (index >= library->relocations.size())
    return fail(TDYN_NOT_FOUND, "relocation index out of range");
  const auto& r = library->relocations[index];
  std::memset(out, 0, sizeof(*out));
  out->offset = r.offset;
  out->addend = r.addend;
  out->native_type = r.nativeType;
  out->type_name = r.typeName.c_str();
  out->bit_size = r.bitSize;
  out->pc_relative = r.pcRelative ? 1 : 0;
  if (r.symbol) {
    out->has_symbol = 1;
    out->symbol_index = *r.symbol;
    out->symbol_table = r.dynamicSymbol ? TDYN_SYMBOL_TABLE_DYNAMIC
                                        : TDYN_SYMBOL_TABLE_STATIC;
    const auto& symbols = r.dynamicSymbol ? library->object->dynamicSymbols()
                                          : library->object->symbols();
    if (*r.symbol < symbols.size()) out->symbol_name = symbols[*r.symbol].name.c_str();
  }
  if (r.targetSection) {
    out->has_target_section = 1;
    out->target_section = *r.targetSection;
  }
  return ok();
}

tdyn_status_t tdyn_library_file_offset_for_address(const tdyn_library_t* library,
                                                   uint64_t address,
                                                   uint64_t* out) {
  if (!library || !out)
    return fail(TDYN_INVALID_ARGUMENT, "library and output are required");
  if (!library->object)
    return fail(TDYN_INSPECT_ERROR, "library has no qBFD inspection data");
  auto value = library->object->fileOffsetForAddress(address);
  if (!value) return fail(TDYN_NOT_FOUND, "address is not file-backed");
  *out = *value;
  return ok();
}

tdyn_status_t tdyn_library_bytes_at_address(const tdyn_library_t* library,
                                            uint64_t address,
                                            uint64_t length,
                                            const uint8_t** out_bytes,
                                            size_t* out_size) {
  if (!library || !out_bytes || !out_size)
    return fail(TDYN_INVALID_ARGUMENT, "library, bytes and size are required");
  if (!library->object)
    return fail(TDYN_INSPECT_ERROR, "library has no qBFD inspection data");
  auto bytes = library->object->bytesAtAddress(address, length);
  if (!bytes) return fail(TDYN_NOT_FOUND, bytes.error().message);
  *out_bytes = bytes->data();
  *out_size = bytes->size();
  return ok();
}

tdyn_status_t tdyn_env_insert_symbol(tdyn_env_t* env, const char* name,
                                     void* address) {
  if (!env || !name || !*name)
    return fail(TDYN_INVALID_ARGUMENT, "environment and symbol name are required");
  try {
    env->symbols[std::string(name)] = EnvSymbol{address, nullptr};
    return ok();
  } catch (const std::bad_alloc&) {
    return fail(TDYN_OUT_OF_MEMORY, "cannot insert symbol");
  }
}

tdyn_status_t tdyn_env_promote(tdyn_env_t* env, tdyn_library_t* library,
                               tdyn_symbol_table_t table) {
  if (!env || !library)
    return fail(TDYN_INVALID_ARGUMENT, "environment and library are required");
  if (library->owner != env)
    return fail(TDYN_INVALID_ARGUMENT, "promoted library must belong to the environment");
  if (!library->object)
    return fail(TDYN_INSPECT_ERROR, "library has no qBFD inspection data");
  if (!library->native.handle)
    return fail(TDYN_UNSUPPORTED, "library is not loaded by the native loader");
  auto promote = [&](tdyn_symbol_table_t which) {
    const auto* symbols = symbolTable(library, which);
    if (!symbols) return;
    for (const auto& symbol : *symbols) {
      if (symbol.name.empty()) continue;
      if (symbol.flags & qbfd::sym::Undefined) continue;
      void* address = nullptr;
      if (!nativeSymbol(library->native.handle, symbol.name.c_str(), &address, nullptr))
        continue;
      env->symbols[symbol.name] = EnvSymbol{address, library};
    }
  };
  try {
    if (table == TDYN_SYMBOL_TABLE_ANY || table == TDYN_SYMBOL_TABLE_STATIC)
      promote(TDYN_SYMBOL_TABLE_STATIC);
    if (table == TDYN_SYMBOL_TABLE_ANY || table == TDYN_SYMBOL_TABLE_DYNAMIC)
      promote(TDYN_SYMBOL_TABLE_DYNAMIC);
    if (table != TDYN_SYMBOL_TABLE_ANY && table != TDYN_SYMBOL_TABLE_STATIC &&
        table != TDYN_SYMBOL_TABLE_DYNAMIC)
      return fail(TDYN_INVALID_ARGUMENT, "invalid symbol table");
    return ok();
  } catch (const std::bad_alloc&) {
    return fail(TDYN_OUT_OF_MEMORY, "cannot promote symbols");
  }
}

tdyn_status_t tdyn_env_symbol(tdyn_env_t* env, const char* name, void** out,
                              tdyn_library_t** owner) {
  if (out) *out = nullptr;
  if (owner) *owner = nullptr;
  if (!env || !name || !out)
    return fail(TDYN_INVALID_ARGUMENT, "environment, symbol name and output are required");
  auto it = env->symbols.find(name);
  if (it != env->symbols.end()) {
    *out = it->second.address;
    if (owner) *owner = it->second.owner;
    return ok();
  }
  for (auto& library : env->libraries) {
    if (!library->native.handle) continue;
    void* address = nullptr;
    if (nativeSymbol(library->native.handle, name, &address, nullptr)) {
      *out = address;
      if (owner) *owner = library.get();
      return ok();
    }
  }
  return fail(TDYN_NOT_FOUND, "symbol not found in environment: " + std::string(name));
}

}  // extern "C"
