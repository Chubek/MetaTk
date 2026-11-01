#pragma once
#include "../Definition.hpp"
namespace vmweave { namespace examples {
// A deliberately specified example ISA, not an alias for a historical machine.
// 16-bit big-endian words, 16 unsigned byte registers, 256 bytes of data memory.
inline Definition toy_r16() {
    using O=Definition::Op;using R=Definition::Rule;
    Definition d("ToyR16-v1");d.state.register_bits=8;d.state.memory_bytes=256;
    R load{"load",0xf000,0x1000,{}};
    auto rd=load.field(8,4),imm=load.field(0,8);load.write_register(rd,imm);d.rules.push_back(load);
    for(auto pair:{std::make_pair(O::add,0x2000u),std::make_pair(O::sub,0x6000u)}) {
        R arith{pair.first==O::add?"add":"sub",0xf00f,pair.second,{}};
        auto dst=arith.field(8,4),src=arith.field(4,4);
        auto a=arith.read_register(dst),b=arith.read_register(src);
        auto value=arith.binary(pair.first,a,b);arith.write_register(dst,value);d.rules.push_back(arith);
    }
    R store{"store",0xf000,0x3000,{}};
    auto src=store.field(8,4),address=store.field(0,8),value=store.read_register(src);
    store.write_byte(address,value);d.rules.push_back(store);
    R branch{"branch",0xf000,0x4000,{}};
    auto condition_reg=branch.field(8,4),target=branch.field(0,8),condition=branch.read_register(condition_reg);
    branch.branch(condition,target);d.rules.push_back(branch);
    R halt{"halt",0xfff0,0x5000,{}};
    auto result_reg=halt.field(0,4),result=halt.read_register(result_reg);halt.halt(result);d.rules.push_back(halt);
    return d;
}
}}
