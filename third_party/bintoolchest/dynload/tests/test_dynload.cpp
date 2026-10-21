#undef NDEBUG
#include "dynload.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <filesystem>

int main(int argc, char** argv) {
  assert(argc == 2);
  tdyn_library_t* bad = reinterpret_cast<tdyn_library_t*>(1);
  assert(tdyn_library_open(argv[1], TDYN_OPEN_NOW | TDYN_OPEN_LAZY, &bad) == TDYN_INVALID_ARGUMENT);
  assert(bad == nullptr);
  assert(tdyn_library_open(argv[1], 1u << 31, &bad) == TDYN_INVALID_ARGUMENT);
  assert(bad == nullptr);
  // Inspect-only must fail, not return a useless successful handle.
  assert(tdyn_library_open(__FILE__, TDYN_OPEN_INSPECT_ONLY, &bad) == TDYN_INSPECT_ERROR);
  assert(bad == nullptr);
  tdyn_env_t* env = nullptr;
  assert(tdyn_env_create(&env) == TDYN_OK);

  tdyn_library_t* lib = nullptr;
  uint32_t flags = TDYN_OPEN_NOW | TDYN_OPEN_LOCAL | TDYN_OPEN_REQUIRE_INSPECT;
  tdyn_status_t rc = tdyn_env_open(env, argv[1], flags, &lib);
  if (rc != TDYN_OK) {
    std::cerr << tdyn_last_error() << '\n';
    return 1;
  }
  assert(tdyn_library_is_native_loaded(lib));
  assert(tdyn_library_is_inspected(lib));
  assert(tdyn_library_symbol_count(lib, TDYN_SYMBOL_TABLE_ANY) > 0);

  tdyn_symbol_info_t info{};
  assert(tdyn_library_find_symbol_info(lib, "tdynload_fixture_add",
                                       TDYN_SYMBOL_TABLE_ANY, &info) == TDYN_OK);
  assert(info.name && std::strcmp(info.name, "tdynload_fixture_add") == 0);

  void* raw = nullptr;
  assert(tdyn_library_symbol(lib, "tdynload_fixture_add", &raw) == TDYN_OK);
  using AddFn = int (*)(int, int);
  AddFn add = nullptr;
  static_assert(sizeof(add) == sizeof(raw));
  std::memcpy(&add, &raw, sizeof(add));
  assert(add(2, 5) == 7);

  assert(tdyn_env_promote(env, lib, TDYN_SYMBOL_TABLE_DYNAMIC) == TDYN_OK);
  void* promoted = nullptr;
  tdyn_library_t* owner = nullptr;
  assert(tdyn_env_symbol(env, "tdynload_fixture_add", &promoted, &owner) == TDYN_OK);
  assert(promoted == raw);
  assert(owner == lib);

  assert(tdyn_library_section_count(lib) > 0);
  (void)tdyn_library_needed_count(lib);

  size_t needed = 0;
  assert(tdyn_env_add_search_path(env, std::filesystem::path(argv[1]).parent_path().string().c_str()) == TDYN_OK);
  const auto name = std::filesystem::path(argv[1]).filename().string();
  assert(tdyn_env_resolve_library(env, name.c_str(), nullptr, &needed) == TDYN_BUFFER_TOO_SMALL);
  assert(needed > 1);
  char tiny[1] = {'x'};
  size_t capacity = sizeof(tiny);
  assert(tdyn_env_resolve_library(env, name.c_str(), tiny, &capacity) == TDYN_BUFFER_TOO_SMALL);
  assert(capacity == needed && tiny[0] == 'x');

  raw = reinterpret_cast<void*>(1);
  assert(tdyn_library_symbol(lib, "no_such_symbol_12345", &raw) == TDYN_NOT_FOUND);
  assert(raw == nullptr);
  // An environment-owned library can be explicitly closed without dangling
  // promoted entries or a second delete during environment teardown.
  tdyn_library_close(lib);
  assert(tdyn_env_library_count(env) == 0);
  promoted = reinterpret_cast<void*>(1);
  owner = reinterpret_cast<tdyn_library_t*>(1);
  assert(tdyn_env_symbol(env, "tdynload_fixture_add", &promoted, &owner) == TDYN_NOT_FOUND);
  assert(promoted == nullptr && owner == nullptr);

  tdyn_env_destroy(env);
  env = tdyn_env_new();
  assert(env);
  tdyn_env_destroy(env);
  lib = tdyn_library_load(argv[1], flags);
  assert(lib);
  env = tdyn_env_new();
  assert(env);
  assert(tdyn_env_promote(env, lib, TDYN_SYMBOL_TABLE_DYNAMIC) == TDYN_INVALID_ARGUMENT);
  tdyn_env_destroy(env);
  tdyn_library_close(lib);
  return 0;
}
