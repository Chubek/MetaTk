// dynload.hpp - C++ RAII wrapper for Tourchstone dynload.
#pragma once

#include "dynload.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace tourchstone::dynload {

class Error : public std::runtime_error {
 public:
  Error(tdyn_status_t status, std::string message)
      : std::runtime_error(std::move(message)), status_(status) {}
  tdyn_status_t status() const noexcept { return status_; }

 private:
  tdyn_status_t status_ = TDYN_ERROR;
};

inline void check(tdyn_status_t status) {
  if (status == TDYN_OK) return;
  const char* msg = tdyn_last_error();
  if (!msg || !*msg) msg = tdyn_status_string(status);
  throw Error(status, msg ? msg : "dynload error");
}

class Library {
 public:
  Library() = default;
  ~Library() { reset(); }
  Library(const Library&) = delete;
  Library& operator=(const Library&) = delete;
  Library(Library&& other) noexcept { moveFrom(other); }
  Library& operator=(Library&& other) noexcept {
    if (this != &other) {
      reset();
      moveFrom(other);
    }
    return *this;
  }

  static Library open(std::string_view path,
                      uint32_t flags = TDYN_OPEN_NOW | TDYN_OPEN_LOCAL) {
    tdyn_library_t* raw = nullptr;
    check(tdyn_library_open(std::string(path).c_str(), flags, &raw));
    return Library(raw, true);
  }

  static Library self(uint32_t flags = TDYN_OPEN_NOW | TDYN_OPEN_LOCAL) {
    tdyn_library_t* raw = nullptr;
    check(tdyn_library_open_self(flags, &raw));
    return Library(raw, true);
  }

  static Library borrowed(tdyn_library_t* raw) { return Library(raw, false); }

  void reset() noexcept {
    if (owned_ && handle_) tdyn_library_close(handle_);
    handle_ = nullptr;
    owned_ = false;
  }

  tdyn_library_t* get() const noexcept { return handle_; }
  explicit operator bool() const noexcept { return handle_ != nullptr; }

  std::string path() const {
    const char* p = tdyn_library_path(handle_);
    return p ? p : std::string{};
  }
  bool nativeLoaded() const { return tdyn_library_is_native_loaded(handle_) != 0; }
  bool inspected() const { return tdyn_library_is_inspected(handle_) != 0; }
  std::string inspectionError() const {
    const char* e = tdyn_library_inspection_error(handle_);
    return e ? e : std::string{};
  }

  void* symbolRaw(std::string_view name) const {
    void* p = nullptr;
    check(tdyn_library_symbol(handle_, std::string(name).c_str(), &p));
    return p;
  }

  template <class T>
  T symbol(std::string_view name) const {
    return reinterpret_cast<T>(symbolRaw(name));
  }

  std::vector<tdyn_symbol_info_t> symbols(
      tdyn_symbol_table_t table = TDYN_SYMBOL_TABLE_ANY) const {
    std::vector<tdyn_symbol_info_t> out;
    size_t n = tdyn_library_symbol_count(handle_, table);
    out.resize(n);
    for (size_t i = 0; i < n; ++i) check(tdyn_library_symbol_info(handle_, table, i, &out[i]));
    return out;
  }

 private:
  explicit Library(tdyn_library_t* raw, bool owned) : handle_(raw), owned_(owned) {}
  void moveFrom(Library& other) noexcept {
    handle_ = other.handle_;
    owned_ = other.owned_;
    other.handle_ = nullptr;
    other.owned_ = false;
  }

  tdyn_library_t* handle_ = nullptr;
  bool owned_ = false;
};

class Environment {
 public:
  Environment() { check(tdyn_env_create(&env_)); }
  ~Environment() { tdyn_env_destroy(env_); }
  Environment(const Environment&) = delete;
  Environment& operator=(const Environment&) = delete;
  Environment(Environment&& other) noexcept : env_(other.env_) {
    other.env_ = nullptr;
  }
  Environment& operator=(Environment&& other) noexcept {
    if (this != &other) {
      tdyn_env_destroy(env_);
      env_ = other.env_;
      other.env_ = nullptr;
    }
    return *this;
  }

  tdyn_env_t* get() const noexcept { return env_; }

  void addSearchPath(std::string_view path) {
    check(tdyn_env_add_search_path(env_, std::string(path).c_str()));
  }
  void addDefaultSearchPaths() { check(tdyn_env_add_default_search_paths(env_)); }

  Library open(std::string_view path,
               uint32_t flags = TDYN_OPEN_NOW | TDYN_OPEN_LOCAL) {
    tdyn_library_t* raw = nullptr;
    check(tdyn_env_open(env_, std::string(path).c_str(), flags, &raw));
    return Library::borrowed(raw);
  }

  Library self(uint32_t flags = TDYN_OPEN_NOW | TDYN_OPEN_LOCAL) {
    tdyn_library_t* raw = nullptr;
    check(tdyn_env_open_self(env_, flags, &raw));
    return Library::borrowed(raw);
  }

  void promote(Library& library,
               tdyn_symbol_table_t table = TDYN_SYMBOL_TABLE_DYNAMIC) {
    check(tdyn_env_promote(env_, library.get(), table));
  }

  void insertSymbol(std::string_view name, void* address) {
    check(tdyn_env_insert_symbol(env_, std::string(name).c_str(), address));
  }

  void* symbolRaw(std::string_view name, Library* owner = nullptr) {
    void* p = nullptr;
    tdyn_library_t* rawOwner = nullptr;
    check(tdyn_env_symbol(env_, std::string(name).c_str(), &p,
                          owner ? &rawOwner : nullptr));
    if (owner) *owner = Library::borrowed(rawOwner);
    return p;
  }

  template <class T>
  T symbol(std::string_view name) {
    return reinterpret_cast<T>(symbolRaw(name));
  }

 private:
  tdyn_env_t* env_ = nullptr;
};

}  // namespace tourchstone::dynload

namespace tdyn = tourchstone::dynload;
