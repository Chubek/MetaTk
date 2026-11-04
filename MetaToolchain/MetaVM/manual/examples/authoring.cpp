#include "vmweave/Builder.hpp"
#include "vmweave/examples/ToyR16.hpp"
#include "vmweave/vmweave.h"
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "usage: manual_authoring [OUTPUT.c]\n";
        return 2;
    }
    try {
        vmweave::Builder builder;
        builder.function("main", 0, 3);
        builder.constant(0, 6);
        builder.constant(1, 7);
        builder.binary("mul", 2, 0, 1);
        builder.ret(2);
        builder.end();
        auto module = builder.finish();
        metavm::VM vm;
        auto answer = vm.run(module);
        if (answer.kind != MVM_INT || answer.as.integer != 42)
            throw std::runtime_error("Builder result mismatch");

        auto definition = vmweave::examples::toy_r16();
        const std::vector<uint8_t> code{0x10,40, 0x11,2, 0x20,0x10, 0x30,3, 0x50,0};
        for (auto engine : {MVM_ENGINE_BYTECODE, MVM_ENGINE_THREADED}) {
            auto result = definition.execute(code, {}, {}, mvm_default_limits(), engine);
            if (result.value != 42 || result.registers[0] != 42 || result.memory[3] != 42)
                throw std::runtime_error("Definition result mismatch");
        }
        if (argc == 2) {
            auto lowered = definition.lower(code);
            mvm_error error{};
            metavm::check(vmwv_emit_c_file(lowered.get(), "manual_toy_factory", argv[1], &error), error);
        }
        std::cout << "builder=42 definition=42 memory[3]=42\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
