#include "dynload.hpp"

#include <options.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using qobj::OptionSpec;

namespace {

constexpr OptionSpec kHelp{'h', "help", false, "", "show this help"};
constexpr OptionSpec kLibraryPath{'L', "library-path", true, "dir", "add a library search directory"};
constexpr OptionSpec kDefaultPaths{0, "default-paths", false, "", "add platform library-path environment variables"};
constexpr OptionSpec kSelf{0, "self", false, "", "open the current process instead of a path"};
constexpr OptionSpec kInspectOnly{0, "inspect-only", false, "", "parse with qBFD without native loading"};
constexpr OptionSpec kRequireInspect{0, "require-inspect", false, "", "fail if qBFD cannot inspect the file"};
constexpr OptionSpec kLazy{0, "lazy", false, "", "use lazy binding"};
constexpr OptionSpec kNow{0, "now", false, "", "use immediate binding (default)"};
constexpr OptionSpec kLocal{0, "local", false, "", "keep symbols local to the handle (default)"};
constexpr OptionSpec kGlobal{0, "global", false, "", "make symbols globally visible to later loads"};
constexpr OptionSpec kNoDelete{0, "nodelete", false, "", "ask the host loader not to unload the image"};
constexpr OptionSpec kNoLoad{0, "noload", false, "", "look up an already-loaded image"};
constexpr OptionSpec kDeepBind{0, "deepbind", false, "", "prefer the image's own definitions where supported"};
constexpr OptionSpec kInspect{'i', "inspect", false, "", "print qBFD identity metadata"};
constexpr OptionSpec kNeeded{'n', "needed", false, "", "print shared-library dependencies"};
constexpr OptionSpec kSymbols{'s', "symbols", false, "", "list dynamic symbols"};
constexpr OptionSpec kAllSymbols{'a', "all-symbols", false, "", "list static and dynamic symbols"};
constexpr OptionSpec kStaticSymbols{0, "static-symbols", false, "", "list the static symbol table"};
constexpr OptionSpec kSections{0, "sections", false, "", "list sections"};
constexpr OptionSpec kRelocs{0, "relocs", false, "", "list relocations"};
constexpr OptionSpec kResolve{'r', "resolve", true, "symbol", "resolve a symbol with the native loader"};
constexpr OptionSpec kPromote{'p', "promote", false, "", "promote loadable symbols into the dynload environment"};
constexpr OptionSpec kEnvSymbol{0, "env-symbol", true, "symbol", "resolve a symbol from the dynload environment"};
constexpr OptionSpec kAddress{0, "address", true, "addr", "find the qBFD symbol covering an address"};

std::string hex(uint64_t value, unsigned width = 0) {
  std::ostringstream out;
  out << "0x" << std::hex << std::setfill('0');
  if (width) out << std::setw(width);
  out << value;
  return out.str();
}

std::string ptr(void* value) {
  return hex(reinterpret_cast<std::uintptr_t>(value), sizeof(void*) * 2);
}

bool parseNumber(const std::string& text, uint64_t& out) {
  try {
    size_t used = 0;
    out = std::stoull(text, &used, 0);
    return used == text.size();
  } catch (...) {
    return false;
  }
}

uint32_t openFlags(const qobj::Options& opts) {
  uint32_t flags = 0;
  flags |= opts.has("lazy") && !opts.has("now") ? TDYN_OPEN_LAZY : TDYN_OPEN_NOW;
  flags |= opts.has("global") ? TDYN_OPEN_GLOBAL : TDYN_OPEN_LOCAL;
  if (opts.has("nodelete")) flags |= TDYN_OPEN_NODELETE;
  if (opts.has("noload")) flags |= TDYN_OPEN_NOLOAD;
  if (opts.has("deepbind")) flags |= TDYN_OPEN_DEEPBIND;
  if (opts.has("require-inspect")) flags |= TDYN_OPEN_REQUIRE_INSPECT;
  if (opts.has("inspect-only")) flags |= TDYN_OPEN_INSPECT_ONLY;
  return flags;
}

tdyn_symbol_table_t selectedTable(const qobj::Options& opts) {
  if (opts.has("all-symbols")) return TDYN_SYMBOL_TABLE_ANY;
  if (opts.has("static-symbols")) return TDYN_SYMBOL_TABLE_STATIC;
  return TDYN_SYMBOL_TABLE_DYNAMIC;
}

void printInspect(tdyn_library_t* lib) {
  std::cout << "path: " << (tdyn_library_path(lib) ? tdyn_library_path(lib) : "") << '\n';
  std::cout << "native-loaded: " << (tdyn_library_is_native_loaded(lib) ? "yes" : "no") << '\n';
  if (!tdyn_library_is_inspected(lib)) {
    std::cout << "qBFD: unavailable";
    const char* err = tdyn_library_inspection_error(lib);
    if (err && *err) std::cout << " (" << err << ')';
    std::cout << '\n';
    return;
  }
  std::cout << "format: " << tdyn_library_format_name(lib) << '\n';
  std::cout << "arch: " << tdyn_library_arch_name(lib) << '\n';
  std::cout << "endian: " << tdyn_library_endian_name(lib) << '\n';
  std::cout << "file-type: " << tdyn_library_file_type_name(lib) << '\n';
  std::cout << "entry: " << hex(tdyn_library_entry_point(lib)) << '\n';
  std::cout << "image-base: " << hex(tdyn_library_image_base(lib)) << '\n';
}

void printNeeded(tdyn_library_t* lib) {
  size_t count = tdyn_library_needed_count(lib);
  std::cout << "needed libraries:" << '\n';
  for (size_t i = 0; i < count; ++i) std::cout << "  " << tdyn_library_needed(lib, i) << '\n';
}

void printSymbols(tdyn_library_t* lib, tdyn_symbol_table_t table) {
  size_t count = tdyn_library_symbol_count(lib, table);
  std::cout << "symbols:" << '\n';
  for (size_t i = 0; i < count; ++i) {
    tdyn_symbol_info_t s{};
    if (tdyn_library_symbol_info(lib, table, i, &s) != TDYN_OK) continue;
    char tableChar = s.table == TDYN_SYMBOL_TABLE_DYNAMIC ? 'D' : 'S';
    std::cout << "  " << tableChar << ' ' << std::setw(18) << hex(s.value)
              << " size=" << std::dec << s.size
              << " bind=" << tdyn_symbol_binding_name(s.binding)
              << " kind=" << tdyn_symbol_kind_name(s.kind);
    if (s.undefined) std::cout << " undefined";
    if (s.exported) std::cout << " exported";
    if (s.imported) std::cout << " imported";
    if (s.hidden) std::cout << " hidden";
    std::cout << "  " << (s.name ? s.name : "") << '\n';
  }
}

void printSections(tdyn_library_t* lib) {
  std::cout << "sections:" << '\n';
  for (size_t i = 0, n = tdyn_library_section_count(lib); i < n; ++i) {
    tdyn_section_info_t s{};
    if (tdyn_library_section_info(lib, i, &s) != TDYN_OK) continue;
    std::cout << "  [" << s.index << "] " << (s.name ? s.name : "")
              << " vma=" << hex(s.vma) << " size=" << hex(s.size)
              << " file=" << hex(s.file_offset) << "+" << hex(s.file_size)
              << " flags=" << hex(s.flags) << '\n';
  }
}

void printRelocs(tdyn_library_t* lib) {
  std::cout << "relocations:" << '\n';
  for (size_t i = 0, n = tdyn_library_relocation_count(lib); i < n; ++i) {
    tdyn_relocation_info_t r{};
    if (tdyn_library_relocation_info(lib, i, &r) != TDYN_OK) continue;
    std::cout << "  " << hex(r.offset) << " "
              << (r.type_name && *r.type_name ? r.type_name : hex(r.native_type));
    if (r.has_symbol) std::cout << " " << (r.symbol_name ? r.symbol_name : "<symbol>");
    if (r.addend) std::cout << " addend=" << r.addend;
    if (r.pc_relative) std::cout << " pc-relative";
    std::cout << '\n';
  }
}

int failWithLast(const std::string& what) {
  std::cerr << what << ": " << tdyn_last_error() << '\n';
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  qobj::Options opts("tdynload", "[options] <library> [symbol ...]",
                     {kHelp,
                      kLibraryPath,
                      kDefaultPaths,
                      kSelf,
                      kInspectOnly,
                      kRequireInspect,
                      kLazy,
                      kNow,
                      kLocal,
                      kGlobal,
                      kNoDelete,
                      kNoLoad,
                      kDeepBind,
                      kInspect,
                      kNeeded,
                      kSymbols,
                      kAllSymbols,
                      kStaticSymbols,
                      kSections,
                      kRelocs,
                      kResolve,
                      kPromote,
                      kEnvSymbol,
                      kAddress});
  if (!opts.parse(argc, argv)) {
    std::cerr << opts.error() << '\n' << opts.usage();
    return 2;
  }
  if (opts.has(kHelp)) {
    std::cout << opts.usage();
    return 0;
  }

  const auto& positional = opts.positional();
  bool useSelf = opts.has("self");
  if (!useSelf && positional.empty()) {
    std::cerr << opts.usage();
    return 2;
  }

  tdyn_env_t* env = nullptr;
  if (tdyn_env_create(&env) != TDYN_OK) return failWithLast("create environment");
  std::unique_ptr<tdyn_env_t, decltype(&tdyn_env_destroy)> envGuard(env, tdyn_env_destroy);

  if (opts.has("default-paths") && tdyn_env_add_default_search_paths(env) != TDYN_OK)
    return failWithLast("add default paths");
  for (const auto& dir : opts.valueList(kLibraryPath))
    if (tdyn_env_add_search_path(env, dir.c_str()) != TDYN_OK)
      return failWithLast("add search path");

  tdyn_library_t* lib = nullptr;
  uint32_t flags = openFlags(opts);
  tdyn_status_t openStatus = useSelf
                                 ? tdyn_env_open_self(env, flags, &lib)
                                 : tdyn_env_open(env, positional[0].c_str(), flags, &lib);
  if (openStatus != TDYN_OK) return failWithLast("open");

  if (opts.has(kPromote) && tdyn_env_promote(env, lib, selectedTable(opts)) != TDYN_OK)
    return failWithLast("promote");

  bool anyAction = opts.has(kInspect) || opts.has(kNeeded) || opts.has(kSymbols) ||
                   opts.has(kAllSymbols) || opts.has(kStaticSymbols) ||
                   opts.has(kSections) || opts.has(kRelocs) || opts.has(kResolve) ||
                   opts.has(kEnvSymbol) || opts.has(kAddress);
  if (!anyAction && positional.size() <= (useSelf ? 0u : 1u)) {
    printInspect(lib);
    printNeeded(lib);
    return 0;
  }

  if (opts.has(kInspect)) printInspect(lib);
  if (opts.has(kNeeded)) printNeeded(lib);
  if (opts.has(kSymbols) || opts.has(kAllSymbols) || opts.has(kStaticSymbols))
    printSymbols(lib, selectedTable(opts));
  if (opts.has(kSections)) printSections(lib);
  if (opts.has(kRelocs)) printRelocs(lib);

  std::vector<std::string> resolves = opts.valueList(kResolve);
  size_t firstSymbol = useSelf ? 0 : 1;
  for (size_t i = firstSymbol; i < positional.size(); ++i) resolves.push_back(positional[i]);
  for (const auto& name : resolves) {
    void* address = nullptr;
    if (tdyn_library_symbol(lib, name.c_str(), &address) != TDYN_OK)
      return failWithLast("resolve " + name);
    std::cout << name << " = " << ptr(address) << '\n';
  }

  for (const auto& name : opts.valueList(kEnvSymbol)) {
    void* address = nullptr;
    tdyn_library_t* owner = nullptr;
    if (tdyn_env_symbol(env, name.c_str(), &address, &owner) != TDYN_OK)
      return failWithLast("environment resolve " + name);
    std::cout << name << " = " << ptr(address);
    if (owner && tdyn_library_path(owner)) std::cout << " from " << tdyn_library_path(owner);
    std::cout << '\n';
  }

  for (const auto& text : opts.valueList(kAddress)) {
    uint64_t address = 0;
    if (!parseNumber(text, address)) {
      std::cerr << "invalid address: " << text << '\n';
      return 2;
    }
    tdyn_symbol_info_t info{};
    if (tdyn_library_symbol_at(lib, address, selectedTable(opts), &info) != TDYN_OK)
      return failWithLast("address lookup " + text);
    std::cout << text << " -> " << (info.name ? info.name : "<anonymous>")
              << " + " << hex(address - info.value) << '\n';
  }
  return 0;
}
