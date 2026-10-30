// C++11 ownership helpers over the C11 implementation; standard library only.
#ifndef METAVM_HPP
#define METAVM_HPP
#include "MetaVM.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace metavm {
class Error : public std::runtime_error {
public:
    mvm_status status;
    explicit Error(mvm_status s, const mvm_error &e)
        : std::runtime_error(e.message[0] ? e.message : mvm_status_string(s)), status(s) {}
};
inline void check(mvm_status s, const mvm_error &e) { if (s != MVM_OK) throw Error(s, e); }

class Module {
    std::unique_ptr<mvm_module, decltype(&mvm_module_destroy)> module_;
    explicit Module(mvm_module *m) : module_(m, mvm_module_destroy) {}
public:
    Module() : Module(mvm_module_create()) { if (!module_) throw std::bad_alloc(); }
    Module(Module &&) noexcept = default;
    Module &operator=(Module &&) noexcept = default;
    Module(const Module &) = delete;
    Module &operator=(const Module &) = delete;
    static Module compile(const std::string &source) {
        mvm_module *m = nullptr;
        mvm_error e{};
        check(mvm_compile(source.data(), source.size(), 0, &m, &e), e);
        return Module(m);
    }
    static Module load(const std::string &path) {
        mvm_module *m = nullptr;
        mvm_error e{};
        check(mvm_load_file(path.c_str(), 0, &m, &e), e);
        return Module(m);
    }
    mvm_module *get() noexcept { return module_.get(); }
    const mvm_module *get() const noexcept { return module_.get(); }
    std::uint32_t find(const std::string &name) const {
        std::uint32_t index = 0;
        mvm_error e{};
        check(mvm_module_find(get(), name.c_str(), &index), e);
        return index;
    }
    void save(const std::string &path) const {
        mvm_error e{};
        check(mvm_save_file(path.c_str(), get(), &e), e);
    }
};

class Root {
    // Ownership keeps the VM alive until after the root is released.
    std::shared_ptr<mvm_vm> vm_;
    std::unique_ptr<mvm_root, decltype(&mvm_unpin)> root_;
    friend class VM;
    Root(std::shared_ptr<mvm_vm> vm, mvm_root *r) : vm_(std::move(vm)), root_(r, mvm_unpin) {}
public:
    Root(Root &&) noexcept = default;
    Root &operator=(Root &&other) noexcept {
        if (this != &other) {
            root_.reset();
            vm_ = std::move(other.vm_);
            root_ = std::move(other.root_);
        }
        return *this;
    }
    Root(const Root &) = delete;
    Root &operator=(const Root &) = delete;
    mvm_value value() const noexcept { return mvm_root_value(root_.get()); }
};

class VM {
    std::shared_ptr<mvm_vm> vm_;
public:
    explicit VM(const mvm_limits &limits = mvm_default_limits()) {
        mvm_error e{};
        mvm_vm *v = mvm_create(&limits, &e);
        if (!v) throw Error(e.status, e);
        vm_ = std::shared_ptr<mvm_vm>(v, mvm_destroy);
    }
    VM(VM &&) noexcept = default;
    VM &operator=(VM &&) noexcept = default;
    VM(const VM &) = delete;
    VM &operator=(const VM &) = delete;
    mvm_vm *get() const noexcept { return vm_.get(); }
    mvm_value run(const Module &m, std::uint32_t entry = 0,
                  const std::vector<mvm_value> &args = {}, mvm_engine engine = MVM_ENGINE_BYTECODE) {
        mvm_error e{};
        mvm_value result = mvm_nil();
        check(mvm_run(get(), m.get(), entry, args.data(), args.size(), engine, &result, &e), e);
        return result;
    }
    Root pin(mvm_value value) {
        mvm_error e{};
        mvm_root *r = nullptr;
        check(mvm_pin(get(), value, &r), e);
        return Root(vm_, r);
    }
};
} // namespace metavm
#endif
