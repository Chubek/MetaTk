// Linked only to MetaVM::Machinery: this resolves its C++11 wrapper, not the
// framework's different MetaVM.hpp umbrella.
#include "MetaVM.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        auto module = metavm::Module::compile(
            ".function main 0 1\nconst r0 42\nret r0\n.end\n");
        metavm::VM vm;
        auto result = vm.run(module, module.find("main"));
        if (result.kind != MVM_INT || result.as.integer != 42)
            throw std::runtime_error("Unexpected result");

        mvm_error error{};
        mvm_value array = mvm_nil(), text = mvm_nil(), retrieved = mvm_nil();
        metavm::check(mvm_array_new(vm.get(), 1, &array), error);
        auto root = vm.pin(array); // Before the next allocation.
        static const unsigned char bytes[] = {'a', 0, 'b'};
        metavm::check(mvm_string_new(vm.get(), bytes, sizeof bytes, &text), error);
        metavm::check(mvm_array_set(vm.get(), root.value(), 0, text), error);
        mvm_collect(vm.get()); // The rooted array keeps its string element alive.
        metavm::check(mvm_array_get(vm.get(), root.value(), 0, &retrieved), error);
        const unsigned char* data = nullptr;
        size_t length = 0;
        metavm::check(mvm_string_data(vm.get(), retrieved, &data, &length), error);
        if (length != sizeof bytes || std::memcmp(data, bytes, sizeof bytes))
            throw std::runtime_error("String/root example failed");
        std::cout << result.as.integer << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
