#include "VMLibrary/CHIP-8/Chip8.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#define CHECK(x) do{if(!(x))throw std::runtime_error("check failed: " #x);}while(0)
using Chip8=metavm::machines::Chip8;
static Chip8 run(const std::string&path,Chip8::Profile profile) {
    std::ifstream in(path,std::ios::binary);CHECK(in.good());
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)),{});
    Chip8 machine(profile);CHECK(machine.load(rom));
    // Deterministic injected 60 Hz frames: ten instruction attempts per frame.
    // The selected ROMs end in an explicit self-jump, never an unknown opcode.
    for(size_t attempts=0;attempts<60000;++attempts) {
        CHECK(machine.pc<4095);
        unsigned op=(unsigned(machine.memory[machine.pc])<<8)|machine.memory[machine.pc+1];
        if(op==(0x1000u|machine.pc))return machine;
        auto status=machine.step();
        CHECK(status==Chip8::Status::running||status==Chip8::Status::waiting_vblank);
        if(attempts%10==9)machine.tick_60hz();
    }
    throw std::runtime_error("CHIP-8 test ROM did not finish: "+path);
}
static void checkmark(const Chip8&m,unsigned x,unsigned y,bool corax) {
    // Independent oracle: upstream image-ok/flag-ok sprites and output positions
    // from the supplied Octo source, not a snapshot produced by this interpreter.
    const unsigned rows[]={0xa0,0xc0,0x80};
    if(corax){for(unsigned bit=0;bit<3;++bit)CHECK(m.display[y*64+x+bit]==0);++y;}
    for(unsigned row=0;row<3;++row)for(unsigned bit=0;bit<3;++bit)
        if(m.display[(y+row)*64+x+bit]!=((rows[row]>>(7-bit))&1))
            throw std::runtime_error("ROM reported failure at ("+std::to_string(x)+","+std::to_string(y)+")");
}
int main(int argc,char**argv) {
    try {
        CHECK(argc==2);std::string directory=argv[1];
        for(auto profile:{Chip8::Profile::legacy,Chip8::Profile::cosmac_vip,Chip8::Profile::chip48}) {
            auto corax=run(directory+"/3-corax+.ch8",profile);
            for(unsigned x:{11u,27u,43u,59u})for(unsigned y=1;y<=(x==59?16u:26u);y+=5)checkmark(corax,x,y,true);
            auto flags=run(directory+"/4-flags.ch8",profile);
            struct Group {unsigned x,y,count;};
            for(auto group:{Group{27,1,3},Group{49,1,3},Group{5,6,3},Group{27,6,4},Group{49,6,4},
                Group{5,11,3},Group{27,11,4},Group{49,11,3},Group{27,17,4},Group{49,17,4},
                Group{5,22,3},Group{27,22,4},Group{49,22,3},Group{31,28,2}})
                for(unsigned n=0;n<group.count;++n)checkmark(flags,group.x+4*n,group.y,false);
        }
        std::cout<<"Pinned Corax+ and flags ROMs pass in all three CHIP-8 profiles\n";
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
