#include "MetaVM.hpp"
#include "VMLibrary/CHIP-8/CHIP-8.hpp"
#include "VMLibrary/SSAVM/ISSA.hpp"
#include "VMLibrary/SECD/SECD.hpp"
#include "VMLibrary/Krivine/Krivine.hpp"
#include "vmweave/examples/ToyR16.hpp"
#include "VMLibrary/DVI/DVI.hpp"
#include "vmweave/DviReplay.hpp"
#include "vmspecs/Smalltalk-80/Smalltalk-80.h"
#include <stdexcept>
int main() {
    using S=metavm::machines::SECDMachine;using O=S::Op;
    if(std::get<int64_t>(S::run({1,{{{O::constant,42},{O::stop}}},0}).value)!=42)return 1;
    using K=metavm::machines::KrivineMachine;using T=K::Op;
    if(K::run({1,{{T::variable,0},{T::abstraction,0},{T::application,1,1}},2}).value.term!=1)return 1;
    metavm::machines::Chip8 machine(metavm::machines::Chip8::Profile::cosmac_vip);
    if(!machine.load({0x60,42})||machine.step()!=metavm::machines::Chip8::Status::running||machine.v[0]!=42)return 1;
    using I=metavm::machines::ISSA;
    if(I::analyze(I::parse_checked("const 42\nreturn (0)\n")).return_kind!=I::ValueKind::scalar)return 1;
    if(std::get<int64_t>(I::run(I::parse("const 42\nreturn (0)\n")).value)!=42)return 1;
    auto d=vmweave::examples::toy_r16();if(d.execute({0x10,42,0x50,0}).value!=42)return 1;
    if(metavm::library::catalog().size()!=49)return 1;
    // A valid zero-page DVI document exercises the installed native loader and
    // VMWeave replay path without external fonts or filesystem dependencies.
    std::vector<uint8_t> bytes;
    auto word=[&](uint32_t n){for(int shift=24;shift>=0;shift-=8)bytes.push_back(uint8_t(n>>shift));};
    bytes.insert(bytes.end(),{247,2});word(25400000);word(473628672);word(1000);bytes.push_back(0);
    bytes.push_back(248);word(UINT32_MAX);word(25400000);word(473628672);word(1000);
    word(0);word(0);bytes.insert(bytes.end(),{0,0,0,0,249});word(15);
    bytes.insert(bytes.end(),{2,223,223,223,223});
    vmweave::DviReplay replay(metavm::machines::DviMachine::run(bytes),[](const auto&){throw std::runtime_error("unexpected event");});
    auto replay_module=replay.module();metavm::VM replay_vm;mvm_error replay_error{};
    metavm::check(replay.bind(replay_vm.get(),&replay_error),replay_error);
    if(replay_vm.run(replay_module).as.integer!=0)return 1;
    mvm_module* m=nullptr;mvm_error error{};
    if(legacy_Smalltalk_80_example(&m,&error)!=MVM_OK)return 1;
    mvm_module_destroy(m);
    return 0;
}
