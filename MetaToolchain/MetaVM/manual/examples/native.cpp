#include "MetaVM.hpp"
#include "VMLibrary/CHIP-8/Chip8.hpp"
#include "VMLibrary/SSAVM/ISSA.hpp"
#include "VMLibrary/DRMVM/Graphics.hpp"
#include "VMLibrary/JVM/IntegerMethod.hpp"
#include "VMLibrary/FORTH/Forth.hpp"
#include "VMLibrary/SECD/SECDMachine.hpp"
#include "VMLibrary/Krivine/KrivineMachine.hpp"
#include "VMLibrary/DVI/DviMachine.hpp"
#include "vmweave/DviReplay.hpp"
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        using C = metavm::machines::Chip8;
        C chip8(C::Profile::legacy);
        require(chip8.load({0x60,42}), "CHIP-8 load failed");
        require(chip8.step() == C::Status::running && chip8.v[0] == 42, "CHIP-8 result");

        using I = metavm::machines::ISSA;
        auto ssa = I::parse_checked("const 42\nreturn (0)\n");
        require(std::get<int64_t>(I::run(ssa).value) == 42, "SSAVM result");
        require(metavm::machines::jvm_integer_method({0x10,6,0x10,7,0x68,0xac}) == 42, "JVM result");
        require(metavm::machines::forth(": square dup * ; 6 square 6 +") == std::vector<int64_t>{42}, "FORTH result");

        using S = metavm::machines::SECDMachine;
        using SO = S::Op;
        S::Program secd{1, {
            {{SO::constant,42}, {SO::closure,0,1}, {SO::apply}, {SO::stop}},
            {{SO::load}, {SO::ret}}
        }, 0};
        require(std::get<int64_t>(S::run(secd).value) == 42, "SECD result");

        using K = metavm::machines::KrivineMachine;
        using KO = K::Op;
        K::Program krivine{1, {{KO::variable,0}, {KO::abstraction,0}, {KO::application,1,1}}, 2};
        require(K::run(krivine).value.term == 1, "Krivine result");

        metavm::machines::Graphics surface(4, 4); // Must outlive its bound VM.
        auto graphics = metavm::Module::compile(
            ".import drm_clear 1\n.function main 0 2\n"
            "const r0 16711680\nhost r1 drm_clear r0\nret r1\n.end\n");
        metavm::VM graphics_vm;
        mvm_error error{};
        metavm::check(surface.bind(graphics_vm.get(), &error), error);
        graphics_vm.run(graphics);
        require(surface.pixels()[0] == 0xff0000, "DRMVM result");

        // Standard version-2 zero-page DVI: PRE, POST, POST_POST, signature.
        std::vector<uint8_t> bytes;
        auto word = [&](uint32_t n) {
            for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(uint8_t(n >> shift));
        };
        bytes.insert(bytes.end(), {247,2});
        word(25400000); word(473628672); word(1000); bytes.push_back(0);
        bytes.push_back(248); word(UINT32_MAX);
        word(25400000); word(473628672); word(1000); word(0); word(0);
        bytes.insert(bytes.end(), {0,0,0,0,249}); word(15);
        bytes.insert(bytes.end(), {2,223,223,223,223});
        using D = metavm::machines::DviMachine;
        vmweave::DviReplay replay(D::run(bytes), [](const D::Event&) {
            throw std::runtime_error("Zero-page document emitted an event");
        });
        auto replay_module = replay.module();
        metavm::VM replay_vm;
        metavm::check(replay.bind(replay_vm.get(), &error), error);
        auto count = replay_vm.run(replay_module);
        require(count.kind == MVM_INT && count.as.integer == 0, "DVI replay result");
        std::cout << "All eight native profile examples passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
