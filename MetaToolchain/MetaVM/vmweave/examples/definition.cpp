#include "ToyR16.hpp"
#include "vmweave.h"
#include <iostream>
int main(int argc,char**argv) {
    try {
        auto machine=vmweave::examples::toy_r16();
        const std::vector<uint8_t> code={0x10,40,0x11,2,0x20,0x10,0x30,3,0x50,0};
        if(argc==2) {
            auto module=machine.lower(code);mvm_error error{};
            metavm::check(vmwv_emit_c_file(module.get(),"toy_r16_factory",argv[1],&error),error);
        } else if(argc!=1)throw std::runtime_error("usage: vmweave-definition-demo [OUTPUT.c]");
        auto result=machine.execute(code);
        std::cout<<"result="<<result.value<<" memory[3]="<<unsigned(result.memory[3])<<'\n';
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
