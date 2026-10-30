#include "MetaVM.hpp"
#include <iostream>
#include <type_traits>

int main() {
    static_assert(!std::is_copy_constructible<metavm::VM>::value, "VM owns execution state");
    static_assert(!std::is_copy_constructible<metavm::Module>::value, "module owns code");
    try {
        auto module = metavm::Module::compile(".function main 0 1\nconst r0 42\nret r0\n.end");
        metavm::VM vm;
        if (vm.run(module, module.find("main")).as.integer != 42) return 1;
        metavm::VM moved(std::move(vm));
        if (moved.run(module, 0, {}, MVM_ENGINE_THREADED).as.integer != 42) return 2;
        auto root = moved.pin(mvm_int(17));
        {
            metavm::VM temporary;
            mvm_value object;
            if (mvm_string_new(temporary.get(), "owned", 5, &object) != MVM_OK) return 3;
            root = temporary.pin(object);
        }
        if (root.value().kind != MVM_OBJECT) return 4;
        try {
            (void)metavm::Module::compile("bad input");
            return 5;
        } catch (const metavm::Error &e) {
            if (e.status != MVM_FORMAT) return 6;
        }
        std::cout << "C++11 ownership wrapper passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 7;
    }
    return 0;
}
