#include "VMLibrary/SECD/SECDMachine.hpp"
#include "VMLibrary/Krivine/KrivineMachine.hpp"
#include <iostream>
#include <exception>
int main() {
    try {
        using S=metavm::machines::SECDMachine;using O=S::Op;
        S::Program secd{1,{{{O::constant,2},{O::constant,40},{O::closure,0,1},{O::apply},{O::apply},{O::stop}},
            {{O::closure,0,2},{O::ret}},{{O::load,0,1},{O::load},{O::add},{O::ret}}},0};
        using K=metavm::machines::KrivineMachine;using T=K::Op;
        K::Program krivine{1,{{T::variable,0},{T::abstraction,0},{T::application,1,1}},2};
        auto value=S::run(secd);auto normal=K::run(krivine);
        std::cout<<"secd="<<std::get<int64_t>(value.value)<<" krivine-term="<<normal.value.term<<'\n';
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
