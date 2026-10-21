#include "xbind.h"
#include "xbind_qamrpp.hpp"
#include "xbind_codegen.hpp"

#include <cstring>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <string>

#ifndef XBIND_SOURCE_DIR
#define XBIND_SOURCE_DIR "."
#endif

namespace {

bool has_suffix(const std::string &value, const std::string &suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool file_exists(const std::string &path) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error)) return false;
  std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
  return input.good();
}

void print_usage(std::ostream &stream) {
  stream << "usage: xbind <command> [args]\n"
         << "\n"
         << "commands:\n"
         << "  --help                         Show this help.\n"
         << "  --version                      Print the XBind version.\n"
         << "  --list-targets                 List supported output targets.\n"
         << "  --target NAME                  Show one target's scaffold paths.\n"
         << "  --show-paths [ROOT]            Show key XBind source directories.\n"
         << "  --check-layout [ROOT]          Validate the scaffold layout.\n"
         << "  --find-spec LIB [DIR]          Resolve LIB.xbind or LIB.lua.\n"
         << "  --check-spec LIB [DIR]         Execute and validate a Lua spec with QaMRpp.\n"
         << "  --dump-spec LIB [DIR]          Execute and summarize a Lua spec with QaMRpp.\n"
         << "  --dump-ir LIB [DIR]            Write deterministic spec IR as JSON.\n"
         << "  --emit-check LIB [DIR]         Write C++20 exact-signature assertions.\n"
         << "  --emit-lua LIB [DIR]           Write Lua 5.4 glue through DomLuaBind.\n";
}

void print_target(const xbind_target_info &target) {
  std::cout << target.name << "\n";
  std::cout << "  compiler: " << target.compiler_directory << "\n";
  std::cout << "  dombind:  " << target.dombind_directory << "\n";
  std::cout << "  typedefs: " << target.typedef_file << "\n";
  std::cout << "  native:   " << target.native_boundary << "\n";
}

void layout_diagnostic(void *user, const char *message) {
  auto *stream = static_cast<std::ostream *>(user);
  (*stream) << message << "\n";
}

std::string resolve_spec_or_throw(const std::string &library,
                                  const std::string &directory) {
  if ((has_suffix(library, ".xbind") || has_suffix(library, ".lua")) &&
      file_exists(library)) {
    return library;
  }

  char resolved[4096];
  const xbind_status status =
      xbind_resolve_spec(directory.c_str(), library.c_str(), resolved, sizeof(resolved));
  if (status != XBIND_STATUS_OK) {
    throw std::runtime_error(library + ": " + xbind_status_string(status));
  }
  return resolved;
}

int command_spec(int argc, char **argv) {
  if (argc < 3 || argc > 4) {
    std::cerr << "xbind: " << argv[1]
              << " requires a library name or spec path\n";
    return 2;
  }

  const std::string directory = argc >= 4 ? argv[3] : ".";
  try {
    const std::string spec = resolve_spec_or_throw(argv[2], directory);
    const xbind::qamrpp::Manifest manifest = xbind::qamrpp::load_spec_file(spec);
    if (std::strcmp(argv[1], "--dump-ir") == 0) {
      std::cout << xbind::codegen::dump_ir(manifest);
    } else if (std::strcmp(argv[1], "--emit-check") == 0) {
      std::cout << xbind::codegen::emit_check(manifest);
    } else if (std::strcmp(argv[1], "--emit-lua") == 0) {
      const char* data = std::getenv("XBIND_DATA_DIR");
      std::cout << xbind::codegen::emit_lua(manifest, data && *data ? data : XBIND_SOURCE_DIR);
    } else if (std::strcmp(argv[1], "--dump-spec") == 0) {
      std::cout << xbind::qamrpp::dump_manifest(manifest);
    } else {
      std::cout << "XBind spec OK: " << manifest.name << " (" << spec << ")\n";
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "xbind: " << error.what() << "\n";
    return 1;
  }
}

} // namespace

int main(int argc, char **argv) {
  if (argc <= 1 || std::strcmp(argv[1], "--help") == 0) {
    print_usage(std::cout);
    return 0;
  }

  if (std::strcmp(argv[1], "--version") == 0) {
    std::cout << xbind_version() << "\n";
    return 0;
  }

  if (std::strcmp(argv[1], "--list-targets") == 0) {
    for (size_t index = 0; index < xbind_target_count(); ++index) {
      std::cout << xbind_target_at(index)->name << "\n";
    }
    return 0;
  }

  if (std::strcmp(argv[1], "--target") == 0) {
    if (argc < 3) {
      std::cerr << "xbind: --target requires a target name\n";
      return 2;
    }
    const xbind_target_info *target = xbind_find_target(argv[2]);
    if (!target) {
      std::cerr << "xbind: unknown target: " << argv[2] << "\n";
      return 1;
    }
    print_target(*target);
    return 0;
  }

  if (std::strcmp(argv[1], "--show-paths") == 0) {
    const char *root = argc >= 3 ? argv[2] : XBIND_SOURCE_DIR;
    std::cout << "root:     " << root << "\n";
    std::cout << "lxbind:   " << root << "/lxbind\n";
    std::cout << "stdlib:   " << root << "/stdlib\n";
    std::cout << "typedefs: " << root << "/typedefs\n";
    std::cout << "dombind:  " << root << "/dombind\n";
    return 0;
  }

  if (std::strcmp(argv[1], "--check-layout") == 0) {
    const char *root = argc >= 3 ? argv[2] : XBIND_SOURCE_DIR;
    size_t errors = xbind_check_layout(root, layout_diagnostic, &std::cerr);
    if (errors == 0) {
      std::cout << "XBind scaffold layout OK: " << root << "\n";
      return 0;
    }
    std::cerr << "xbind: " << errors << " layout issue(s)\n";
    return 1;
  }

  if (std::strcmp(argv[1], "--find-spec") == 0) {
    if (argc < 3) {
      std::cerr << "xbind: --find-spec requires a library name\n";
      return 2;
    }
    try {
      std::cout << resolve_spec_or_throw(argv[2], argc >= 4 ? argv[3] : ".") << "\n";
      return 0;
    } catch (const std::exception &error) {
      std::cerr << "xbind: " << error.what() << "\n";
      return 1;
    }
  }

  if (std::strcmp(argv[1], "--check-spec") == 0 || std::strcmp(argv[1], "--dump-spec") == 0 ||
      std::strcmp(argv[1], "--dump-ir") == 0 || std::strcmp(argv[1], "--emit-check") == 0 ||
      std::strcmp(argv[1], "--emit-lua") == 0) {
    return command_spec(argc, argv);
  }

  std::cerr << "xbind: unknown command: " << argv[1] << "\n";
  print_usage(std::cerr);
  return 2;
}
