// dyninvoke.hpp - C++ RAII wrapper for Tourchstone dyninvoke.
#pragma once

#include "dyninvoke.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace tourchstone::dyninvoke {

class Error : public std::runtime_error {
 public:
  Error(dinv_status_t status, std::string message)
      : std::runtime_error(std::move(message)), status_(status) {}
  dinv_status_t status() const noexcept { return status_; }

 private:
  dinv_status_t status_ = DINV_ERROR;
};

inline void check(dinv_status_t status) {
  if (status == DINV_OK) return;
  const char* msg = dinv_last_error();
  if (!msg || !*msg) msg = dinv_status_string(status);
  throw Error(status, msg ? msg : "dyninvoke error");
}

class Signature {
 public:
  Signature(dinv_abi_t abi, const dinv_type_t* ret,
            std::vector<const dinv_type_t*> args)
      : args_(std::move(args)) {
    check(dinv_cif_create(abi, ret, args_.size(), args_.data(), &cif_));
  }
  ~Signature() { dinv_cif_destroy(cif_); }
  Signature(const Signature&) = delete;
  Signature& operator=(const Signature&) = delete;

  dinv_cif_t* get() const noexcept { return cif_; }
  dinv_abi_t abi() const noexcept { return dinv_cif_abi(cif_); }
  size_t nargs() const noexcept { return dinv_cif_nargs(cif_); }

  void call(void* func, void* ret, std::vector<void*> args) {
    if (args.size() != args_.size())
      throw Error(DINV_INVALID_ARGUMENT, "argument count mismatch");
    check(dinv_call(cif_, func, ret, args.data()));
  }

 private:
  dinv_cif_t* cif_ = nullptr;
  std::vector<const dinv_type_t*> args_;
};

class Closure {
 public:
  Closure(dinv_cif_t* cif, dinv_closure_fn_t fn, void* user) {
    check(dinv_closure_create(cif, fn, user, &closure_, &fnptr_));
  }
  ~Closure() { dinv_closure_destroy(closure_); }
  Closure(const Closure&) = delete;
  Closure& operator=(const Closure&) = delete;

  void* fn() const noexcept { return fnptr_; }

 private:
  dinv_closure_t* closure_ = nullptr;
  void* fnptr_ = nullptr;
};

}  // namespace tourchstone::dyninvoke

namespace dinvk = tourchstone::dyninvoke;
