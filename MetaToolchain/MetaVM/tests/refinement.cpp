#include "vmweave/examples/ToyR16.hpp"
#include "VMLibrary/SSAVM/ISSA.hpp"
#include "VMLibrary/DRMVM/Graphics.hpp"
#include "VMLibrary/CHIP-8/Chip8.hpp"
#include "vmweave.h"
#include <functional>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("check failed: " #x);}while(0)
static void traps(const std::function<void()>&f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
using vmweave::Definition;
static void definitions() {
    auto d=vmweave::examples::toy_r16();
    std::vector<uint8_t> code={0x10,40,0x11,2,0x20,0x10,0x30,3,0x50,0};
    for(auto engine:{MVM_ENGINE_BYTECODE,MVM_ENGINE_THREADED}) {
        auto result=d.execute(code,{},{},mvm_default_limits(),engine);
        CHECK(result.value==42&&result.registers[0]==42&&result.memory[3]==42&&result.steps>0);
    }
    code={0x10,3,0x11,1,0x60,0x10,0x40,4,0x50,0};CHECK(d.execute(code).value==0);
    code={0x10,255,0x11,2,0x20,0x10,0x50,0};CHECK(d.execute(code).value==1);
    auto little=d;little.state.endian=Definition::Endian::little;
    for(size_t k=0;k<code.size();k+=2)std::swap(code[k],code[k+1]);
    CHECK(little.execute(code).value==1);
    traps([&]{d.lower({0});});traps([&]{d.lower({0xff,0xff});});
    traps([&]{d.lower({0x40,1,0x50,0});}); // Unaligned branch, including dead branch.
    traps([&]{d.lower({0x40,8,0x50,0});});
    traps([&]{d.lower({0x10,1});}); // Missing terminal halt.
    auto bad=d;bad.rules.push_back(bad.rules[0]);traps([&]{bad.verify();});
    bad=d;bad.rules[0].nodes[0].width=32;traps([&]{bad.verify();});
    bad=d;bad.rules[0].nodes[2].a=99;traps([&]{bad.verify();});
    bad=d;bad.rules[0].nodes[0].op=Definition::Op(999);traps([&]{bad.verify();});
    bad=d;bad.state.registers=1;traps([&]{bad.execute({0x11,1,0x50,0});});
    traps([&]{d.lower({0x50,0},1);});
    auto limits=mvm_default_limits();limits.max_steps=20;
    traps([&]{d.execute({0x10,1,0x40,2,0x50,0},{},{},limits);});
    // Runtime-dependent target is explicitly unsupported, rather than guessed.
    bad=d;bad.rules[3].nodes.back()={Definition::Op::jump,2};
    // Rule 3 is store: node 2 is a dynamic register read.
    traps([&]{bad.lower({0x30,0,0x50,0});});
    Definition sign("signed-field");sign.state.instruction_bits=8;sign.state.memory_bytes=0;
    Definition::Rule rule{"signed",0,0,{}};auto v=rule.field(0,8,true);rule.halt(v);sign.rules.push_back(rule);
    CHECK(sign.execute({0xff}).value==-1);
    for(auto op:{Definition::Op::equal,Definition::Op::less}) {
        Definition comparison("comparison");comparison.state.instruction_bits=8;comparison.state.memory_bytes=0;
        Definition::Rule compare{"compare",0,0,{}};
        auto a=compare.constant(0),b=compare.constant(1);
        auto left=compare.read_register(a),right=compare.read_register(b);
        auto result=compare.binary(op,left,right);compare.halt(result);comparison.rules.push_back(compare);
        CHECK(comparison.execute({0},{1,op==Definition::Op::equal?1:2}).value==1);
        CHECK(comparison.execute({0},{2,1}).value==0);
    }
    traps([&]{rule.binary(Definition::Op::halt,0,0);});
    const std::vector<std::pair<Definition::Op,mvm_opcode>> operations={
        {Definition::Op::add,MVM_OP_ADD},{Definition::Op::sub,MVM_OP_SUB},{Definition::Op::mul,MVM_OP_MUL},
        {Definition::Op::div,MVM_OP_DIV},{Definition::Op::mod,MVM_OP_MOD},{Definition::Op::bit_and,MVM_OP_AND},
        {Definition::Op::bit_or,MVM_OP_OR},{Definition::Op::bit_xor,MVM_OP_XOR},{Definition::Op::shl,MVM_OP_SHL},
        {Definition::Op::shr,MVM_OP_SHR},{Definition::Op::equal,MVM_OP_EQ},{Definition::Op::less,MVM_OP_LT}};
    for(auto operation:operations) {
        Definition arithmetic("arithmetic");arithmetic.state.instruction_bits=8;arithmetic.state.memory_bytes=0;
        Definition::Rule r{"arithmetic",0,0,{}};
        auto zero=r.constant(0),one=r.constant(1),left=r.read_register(zero),right=r.read_register(one);
        r.halt(r.binary(operation.first,left,right));arithmetic.rules.push_back(r);
        for(auto pair:{std::make_pair(int64_t(-7),int64_t(3)),std::make_pair(INT64_MAX,int64_t(1)),std::make_pair(INT64_MIN,int64_t(63))}) {
            int64_t expected;CHECK(mvm_integer_eval(operation.second,pair.first,pair.second,&expected)==MVM_OK);
            CHECK(arithmetic.execute({0},{pair.first,pair.second}).value==expected);
        }
    }
    Definition memory_machine("memory");memory_machine.state.instruction_bits=8;memory_machine.state.memory_bytes=1;
    Definition::Rule mem{"memory",0,0,{}};
    auto zero=mem.constant(0),wide=mem.constant(300);mem.write_byte(zero,wide);mem.halt(mem.read_byte(zero));memory_machine.rules.push_back(mem);
    CHECK(memory_machine.execute({0}).value==44);
    Definition wide_machine("wide");wide_machine.state.instruction_bits=32;wide_machine.state.memory_bytes=0;
    Definition::Rule wide_rule{"wide",0,0,{}};wide_rule.halt(wide_rule.field(0,32));wide_machine.rules.push_back(wide_rule);
    CHECK(wide_machine.execute({0x12,0x34,0x56,0x78}).value==0x12345678);
    Definition unconditional("jump");unconditional.state.instruction_bits=8;unconditional.state.memory_bytes=0;
    Definition::Rule go{"go",255,0,{}};go.jump(go.constant(2));unconditional.rules.push_back(go);
    Definition::Rule stop{"stop",255,1,{}};stop.halt(stop.constant(42));unconditional.rules.push_back(stop);
    CHECK(unconditional.execute({0,0,1}).value==42);
}
static mvm_status callback(mvm_vm*,const mvm_value*,size_t,mvm_value*r,void*,mvm_error*){*r=mvm_int(19);return MVM_OK;}
static mvm_status during_execution(mvm_vm*v,const mvm_value*,size_t,mvm_value*r,void*,mvm_error*) {
    mvm_error e{};if(mvm_register_natives(v,nullptr,0,&e)!=MVM_BUSY)return MVM_HOST_ERROR;*r=mvm_int(1);return MVM_OK;
}
static void binding_transactions() {
    metavm::VM vm;mvm_error e{};
    CHECK(mvm_register_native(vm.get(),"existing",0,callback,nullptr,&e)==MVM_OK);
    const mvm_native_binding bad[]={{"candidate",0,callback,nullptr},{"existing",0,callback,nullptr}};
    CHECK(mvm_register_natives(vm.get(),bad,2,&e)==MVM_INVALID);
    CHECK(mvm_register_native(vm.get(),"candidate",0,callback,nullptr,&e)==MVM_OK);
    const mvm_native_binding duplicates[]={{"same",0,callback,nullptr},{"same",0,callback,nullptr}};
    CHECK(mvm_register_natives(vm.get(),duplicates,2,&e)==MVM_INVALID);
    CHECK(mvm_register_native(vm.get(),"same",0,callback,nullptr,&e)==MVM_OK);
    const mvm_native_binding invalid[]={{"fresh",0,callback,nullptr},{"bad-name",0,callback,nullptr}};
    CHECK(mvm_register_natives(vm.get(),invalid,2,&e)==MVM_INVALID);
    CHECK(mvm_register_native(vm.get(),"fresh",0,callback,nullptr,&e)==MVM_OK);
    CHECK(mvm_register_natives(vm.get(),nullptr,0,&e)==MVM_OK);
    CHECK(mvm_register_natives(nullptr,nullptr,0,&e)==MVM_INVALID);
    CHECK(mvm_register_natives(vm.get(),nullptr,1,&e)==MVM_INVALID);
    CHECK(mvm_register_native(vm.get(),"busy_probe",0,during_execution,nullptr,&e)==MVM_OK);
    auto module=metavm::Module::compile(".import busy_probe 0\n.function main 0 1\nhost r0 busy_probe r0\nret r0\n.end\n");
    CHECK(vm.run(module).as.integer==1);
    // DRMVM's failed two-import bind must not leave drm_clear behind.
    metavm::VM graphics_vm;metavm::machines::Graphics surface(2,2);
    CHECK(mvm_register_native(graphics_vm.get(),"drm_rect",5,callback,nullptr,&e)==MVM_OK);
    CHECK(surface.bind(graphics_vm.get(),&e)==MVM_INVALID);
    CHECK(mvm_register_native(graphics_vm.get(),"drm_clear",1,callback,nullptr,&e)==MVM_OK);
}
static void ssa_text() {
    using I=metavm::machines::ISSA;
    auto p=I::parse("# Figure 5 subset\n0 const 0\n1 const 1\n2 const 10\n3 const 2\n"
        "4 phi (1) (8)\n5 phi (0) (4)\n6 phi (3) (9)\n7 pfe\n"
        "8 iadd (4) (5)\n9 iadd (1) (6)\n10 ble (9) (2) [4] 1\n11 return (8)\n");
    CHECK(std::get<int64_t>(I::run(p).value)==55);
    CHECK(I::analyze(p).return_kind==I::ValueKind::scalar);
    CHECK(std::get<int64_t>(I::run(I::parse("int_arg\nreturn (0)\n"),{42}).value)==42);
    auto source=I::format(p);CHECK(I::format(I::parse(source))==source);
    for(const auto&text:{"0 wat\n","1 const 0\n","const 1 extra\n","const 9223372036854775808\n",
        "const 1\nreturn (-1)\n","const 1\nreturn (2)\n","phi (0)\nreturn (0)\n"})traps([&]{I::parse(text);});
    traps([]{I::run({{I::Op(999),{}}});});
    auto bad=p;bad[10].target=5;traps([&]{I::verify(bad);});
    try{I::parse("\n\nwat\n");CHECK(false);}catch(const std::exception&e){CHECK(std::string(e.what()).find("line 3")!=std::string::npos);}
}
static void builder_validation() {
    vmweave::Builder b;auto old=b.source();traps([&]{b.constant(0,1);});CHECK(b.source()==old);
    b.function("main",0,1);old=b.source();traps([&]{b.constant(1,1);});CHECK(b.source()==old);
    b.constant(0,42);b.ret(0);b.end();auto m=b.finish();metavm::VM vm;CHECK(vm.run(m).as.integer==42);
    vmweave::Builder factorial;factorial.function("main",0,5);
    factorial.constant(0,1);factorial.constant(1,10);factorial.constant(2,1);factorial.constant(3,0);
    factorial.label("loop");factorial.binary("mul",0,0,1);factorial.binary("sub",1,1,2);
    factorial.binary("eq",4,1,3);factorial.jump_false(4,"loop");factorial.ret(0);factorial.end();
    auto fm=factorial.finish();CHECK(vm.run(fm).as.integer==3628800);
}
static void ssa_extensions() {
    using I=metavm::machines::ISSA;using O=I::Op;
    auto run=[](const std::string&source,const std::vector<int64_t>&args) {
        auto p=I::parse(source);CHECK(I::format(I::parse(I::format(p)))==I::format(p));
        CHECK(I::analyze(p).return_kind==I::ValueKind::scalar);
        return std::get<int64_t>(I::run(p,args).value);
    };
    for(auto pair:{std::make_pair(int64_t(0),int64_t(0)),std::make_pair(int64_t(-7),int64_t(0)),
                  std::make_pair(int64_t(2),int64_t(4)),std::make_pair(INT64_MIN,INT64_MAX)}) {
        const std::string head="int_arg\nint_arg\n";
        CHECK(run(head+"and (0) (1)\nreturn (2)\n",{pair.first,pair.second})==int64_t(pair.first!=0&&pair.second!=0));
        CHECK(run(head+"or (0) (1)\nreturn (2)\n",{pair.first,pair.second})==int64_t(pair.first!=0||pair.second!=0));
    }
    CHECK(run("int_arg\nneg (0)\nreturn (1)\n",{INT64_MIN})==INT64_MIN);
    CHECK(run("int_arg\nneg (0)\nreturn (1)\n",{-42})==42);
    for(const auto*op:{"beq","ble","bne","blt","bge","bgt"}) {
        for(int64_t a:{INT64_MIN,int64_t(-1),int64_t(0),INT64_MAX})for(int64_t b:{INT64_MIN,int64_t(0),INT64_MAX}) {
            std::string name=op;bool taken=name=="beq"?a==b:name=="ble"?a<=b:name=="bne"?a!=b:name=="blt"?a<b:name=="bge"?a>=b:a>b;
            CHECK(run("int_arg\nint_arg\nconst 11\nconst 22\n"+name+
                " (0) (1) [6] 1\ngoto [6] 0\nphi (2) (3)\npfe\nreturn (6)\n",{a,b})==(taken?22:11));
        }
    }
    // Appendix B.1's nested factorial loops, adapted to this subset's syntax,
    // i64 values and value-returning API (rather than process exit).
    const std::string factorial="const 0\nconst 1\nint_arg\nint_arg\nphi (0) (12)\npfe\n"
        "phi (1) (9)\nphi (1) (10)\npfe\nmul (6) (7)\nadd (7) (1)\nble (10) (2) [6] 1\n"
        "add (4) (1)\nblt (12) (3) [4] 1\nreturn (9)\n";
    int64_t expected=1;
    for(int64_t n=1;n<=12;++n) {
        expected*=n;for(int64_t repeats=1;repeats<=3;++repeats)CHECK(run(factorial,{n,repeats})==expected);
    }
    // Appendix B.3's persistent-array Fibonacci recurrence, one outer iteration.
    const std::string fib="const 0\nconst 1\nconst 2\nint_arg\nadd (3) (1)\nnewarray (4)\n"
        "update (5) (1) (1)\nphi (2) (16)\nphi (6) (15)\npfe\nsub (7) (1)\nsub (7) (2)\n"
        "access (8) (10)\naccess (8) (11)\nadd (12) (13)\nupdate (8) (7) (14)\n"
        "add (7) (1)\nble (16) (3) [7] 1\naccess (15) (3)\nreturn (18)\n";
    int64_t previous=0,current=1;
    for(int64_t n=2;n<=46;++n){auto next=previous+current;previous=current;current=next;CHECK(run(fib,{n})==current);}

    auto arrays=I::parse("const 2\narray (0)\nconst 0\nconst 7\nset (1) (2) (3)\n"
        "get (1) (2)\nget (4) (2)\nadd (5) (6)\nreturn (7)\n");
    I::Limits limits;limits.max_array_elements=2;limits.max_allocated_elements=4;limits.max_array_allocations=2;limits.max_steps=9;
    CHECK(std::get<int64_t>(I::run_limited(arrays,limits).value)==7);
    limits.max_allocated_elements=3;traps([&]{I::run_limited(arrays,limits);});
    limits.max_allocated_elements=4;limits.max_array_allocations=1;traps([&]{I::run_limited(arrays,limits);});
    limits.max_array_allocations=2;limits.max_array_elements=1;traps([&]{I::run_limited(arrays,limits);});
    limits.max_array_elements=2;limits.max_steps=8;traps([&]{I::run_limited(arrays,limits);});
    auto empty=I::parse("const 0\narray (0)\ngoto [1] 0\n");
    limits=I::Limits{};limits.max_array_elements=0;limits.max_allocated_elements=0;limits.max_array_allocations=2;
    try{I::run_limited(empty,limits);CHECK(false);}catch(const std::runtime_error&e){CHECK(std::string(e.what()).find("allocation count")!=std::string::npos);}
    auto p=I::parse("const 42\nreturn (0)\n");
    limits=I::Limits{};limits.max_steps=0;traps([&]{I::run_limited(p,limits);});
    limits=I::Limits{};limits.max_instructions=1;traps([&]{I::verify(p,limits);});
    traps([&]{I::parse("const 42\nreturn (0)\n",limits);});
    limits.max_instructions=2;limits.max_operands=0;traps([&]{I::verify(p,limits);});
    traps([&]{I::parse("const 42\nreturn (0)\n",limits);});
    limits.max_operands=1;I::verify(p,limits);CHECK(I::parse(I::format(p,limits),limits).size()==2);
    limits.max_source_bytes=18;traps([&]{I::parse("const 42\nreturn (0)\n",limits);});
    limits=I::Limits{};limits.max_line_bytes=10;CHECK(I::parse("const 42\nreturn (0)\n",limits).size()==2);
    limits.max_line_bytes=9;traps([&]{I::parse("const 42\nreturn (0)\n",limits);});
    limits=I::Limits{};limits.max_tokens_per_line=1;traps([&]{I::parse("const 42\nreturn (0)\n",limits);});
    for(const auto&text:{"const 1\n","int_arg -1\nreturn (0)\n","goto [1] 0\nreturn (0)\n",
        "const 0\narray (0)\nand (0) (1)\nreturn (2)\n","const 0\narray (0)\nor (1) (0)\nreturn (2)\n"})
        traps([&]{I::run(I::parse(text));});
    try{I::parse("# comment\n\ngoto [1] 0\nreturn (0)\n");CHECK(false);}
    catch(const std::exception&e){CHECK(std::string(e.what()).find("line 4")!=std::string::npos);}
    // All added branch types retain PHI-group entry validation.
    for(auto op:{O::branch_ne,O::branch_lt,O::branch_ge,O::branch_gt}) {
        std::vector<I::Instruction> bad={{O::constant,{},0},{op,{0,0},0,3,0},
            {O::phi,{0}},{O::pfe,{}},{O::ret,{2}}};traps([&]{I::verify(bad);});
    }
}
static void ssa_analysis() {
    using I=metavm::machines::ISSA;using O=I::Op;using K=I::ValueKind;
    auto check=[](const std::string&s){return I::analyze(I::parse_checked(s));};
    auto reject=[](const std::string&s,size_t instruction,const std::string&message) {
        auto p=I::parse(s); // These programs still pass the old structural verifier.
        try{I::analyze(p);CHECK(false);}
        catch(const I::VerificationError&e){CHECK(e.instruction==instruction);CHECK(std::string(e.what()).find(message)!=std::string::npos);}
    };
    reject("return (1)\nconst 7\nreturn (1)\n",0,"undefined");
    reject("const 0\nphi (0)\nphi (1)\npfe\nreturn (2)\n",2,"undefined");
    reject("phi (0)\npfe\nreturn (0)\n",0,"undefined");
    reject("const 0\narray (0)\nneg (1)\nreturn (2)\n",2,"scalar");
    reject("const 0\nlength (0)\nreturn (1)\n",1,"array");
    reject("const 0\narray (0)\nget (1) (1)\nreturn (2)\n",2,"scalar");
    reject("const 0\narray (0)\nset (1) (0) (1)\nreturn (2)\n",2,"scalar");
    reject("const 0\narray (0)\narray (1)\nreturn (2)\n",2,"scalar");
    for(auto op:{O::add,O::sub,O::mul,O::div,O::equal,O::less_equal,O::logical_and,O::logical_or,
                 O::branch_equal,O::branch_le,O::branch_ne,O::branch_lt,O::branch_ge,O::branch_gt}) {
        for(unsigned operand=0;operand<2;++operand) {
            std::vector<I::Instruction> bad={{O::constant,{},0},{O::array,{0}},
                {op,{0,0},0,3,0},{O::ret,{0}}};
            bad[2].operands[operand]=1;
            try{I::analyze(bad);CHECK(false);}
            catch(const I::VerificationError&e){CHECK(e.instruction==2);CHECK(std::string(e.what()).find("scalar")!=std::string::npos);}
        }
    }
    reject("const 7\ngoto [2] 9\nphi (0)\npfe\nreturn (2)\n",2,"CEN 9");
    // Fixed-point iteration must revisit consumers when a backedge widens types.
    reject("const 0\narray (0)\nphi (0) (1)\npfe\nneg (2)\ngoto [2] 1\n",4,"scalar");
    auto dead=check("const 7\nreturn (0)\nlength (0)\nreturn (2)\n");
    CHECK(dead.reachable==std::vector<bool>({true,true,false,false}));
    CHECK(dead.results==std::vector<K>({K::scalar,K::none,K::none,K::none}));
    CHECK(check("const 7\nphi (0) (4)\npfe\nreturn (1)\nconst 9\nreturn (4)\n").return_kind==K::scalar);
    // Unselected PHI alternatives need not have executed. A CEN persists across
    // ordinary instructions, but both PFE and a conditional fallthrough reset it.
    CHECK(check("const 0\nconst 7\ngoto [4] 1\nreturn (0)\nconst 9\nphi (1) (4)\npfe\nreturn (5)\n").return_kind==K::scalar);
    CHECK(check("const 7\ngoto [2] 99\npfe\nphi (0)\npfe\nreturn (3)\n").return_kind==K::scalar);
    CHECK(check("const 0\ngoto [2] 99\nbne (0) (0) [6] 0\nphi (0)\npfe\nreturn (3)\nreturn (0)\n").return_kind==K::scalar);
    // Scalar/array PHIs may be returned, but not consumed by scalar-only ops.
    const std::string mixed="int_arg\nconst 0\narray (1)\nbeq (0) (1) [5] 1\ngoto [5] 0\nphi (1) (2)\npfe\n";
    CHECK(check(mixed+"return (5)\n").return_kind==K::either);
    reject(mixed+"add (5) (1)\nreturn (7)\n",7,"scalar");
    // Differential diamonds: independently execute both paths for scalar/array
    // alternatives and check the analyzer's union and branch-specific PHI inputs.
    for(unsigned types=0;types<4;++types) {
        auto p=I::parse("int_arg\nconst 0\nbeq (0) (1) [5] 1\nconst 7\ngoto [7] 0\nconst 9\ngoto [7] 1\nphi (3) (5)\npfe\nreturn (7)\n");
        if(types&1)p[3]={O::array,{1}};
        if(types&2)p[5]={O::array,{1}};
        auto analysis=I::analyze(p);
        CHECK(analysis.return_kind==(types==0?K::scalar:types==3?K::array:K::either));
        CHECK(analysis.results[7]==analysis.return_kind&&analysis.results[8]==K::none);
        for(int64_t input:{int64_t(-1),int64_t(0),int64_t(1)}) {
            auto value=I::run(p,{input}).value;bool is_array=types&(input==0?2:1);
            CHECK(std::holds_alternative<I::Array>(value)==is_array);
            if(is_array)CHECK(std::get<I::Array>(value)->empty());
            else CHECK(std::get<int64_t>(value)==(input==0?9:7));
        }
        // Both predecessors claiming edge zero loses the needed PHI correlation.
        p[6].edge=0;traps([&]{I::analyze(p);});
    }
    // Analysis explores both conditional successors, even for constant predicates.
    reject("const 0\nbeq (0) (0) [3] 0\nconst 7\nreturn (2)\n",3,"undefined");
    // Unreachable malformed instructions are still rejected structurally.
    traps([]{I::analyze({{O::constant,{},7},{O::ret,{0}},{O(999),{}}});});
    auto loop=I::parse("goto [0] 0\n");auto stable=I::analyze(loop);
    CHECK(stable.states==1&&stable.return_kind==K::none);
    auto huge=loop;huge[0].edge=SIZE_MAX;
    CHECK(I::analyze(huge).states==2); // Sparse CEN keys, never an edge-indexed array.
    auto p=I::parse("const 7\ngoto [2] 1\nphi (0) (0)\npfe\nreturn (2)\n");
    auto measured=I::analyze(p);I::Limits limits;I::AnalysisLimits budget;
    budget.max_states=measured.states;budget.max_cells=measured.states*p.size();budget.max_work=measured.work;
    CHECK(I::analyze(p,limits,budget).work==measured.work);
    --budget.max_states;traps([&]{I::analyze(p,limits,budget);});++budget.max_states;
    --budget.max_cells;traps([&]{I::analyze(p,limits,budget);});++budget.max_cells;
    --budget.max_work;traps([&]{I::analyze(p,limits,budget);});
    for(unsigned which=0;which<3;++which) {
        budget=I::AnalysisLimits{};
        if(which==0)budget.max_states=0;else if(which==1)budget.max_cells=0;else budget.max_work=0;
        traps([&]{I::analyze(p,limits,budget);});
    }
    // Basic-block facts avoid quadratic storage on long straight-line programs.
    std::vector<I::Instruction> linear(20000,{O::constant,{},1});linear.push_back({O::ret,{19999}});
    budget=I::AnalysisLimits{};budget.max_states=1;budget.max_cells=linear.size();
    CHECK(I::analyze(linear,limits,budget).states==1);
    limits.max_operands=0;traps([&]{I::analyze(p,limits,budget);});
    try{I::parse_checked("# heading\n\nconst 0\n\nlength (0)\nreturn (1)\n");CHECK(false);}
    catch(const std::exception&e){CHECK(std::string(e.what()).find("line 5")!=std::string::npos);}
    budget=I::AnalysisLimits{};budget.max_work=measured.work-1;
    try{I::parse_checked("# heading\n"+I::format(p),I::Limits{},budget);CHECK(false);}
    catch(const std::exception&e){CHECK(std::string(e.what()).find("line ")!=std::string::npos);}
}
static void chip8_profiles() {
    using C=metavm::machines::Chip8;using S=C::Status;using P=C::Profile;using K=C::KeyWait;
    traps([]{C bad(P(999));});
    // Flags are derived from snapshotted operands, including VF as either input.
    for(auto profile:{P::legacy,P::cosmac_vip,P::chip48}) {
        C m(profile);
        for(uint8_t operation:{uint8_t(1),uint8_t(2),uint8_t(3)}) {
            CHECK(m.load({0x80,uint8_t(0xf0|operation)}));m.v[0]=15;m.v[15]=20;
            CHECK(m.step()==S::running);
            CHECK(m.v[0]==(operation==1?31:operation==2?4:27));
            CHECK(m.v[15]==(profile==P::cosmac_vip?0:20));
            CHECK(m.load({0x8f,uint8_t(0x00|operation)}));m.v[0]=15;m.v[15]=20;
            CHECK(m.step()==S::running);
            CHECK(m.v[15]==(profile==P::cosmac_vip?0:operation==1?31:operation==2?4:27));
        }
        CHECK(m.load({0x80,0x16}));m.v[0]=3;m.v[1]=8;
        CHECK(m.step()==S::running);CHECK(m.v[0]==(profile==P::cosmac_vip?4:1));
        CHECK(m.load({0x80,0x1e}));m.v[0]=3;m.v[1]=128;
        CHECK(m.step()==S::running);CHECK(m.v[0]==(profile==P::cosmac_vip?0:6));
        CHECK(m.v[15]==(profile==P::cosmac_vip?1:0));
        CHECK(m.load({0xb2,0x20}));m.v[0]=2;m.v[2]=6;
        CHECK(m.step()==S::running);CHECK(m.pc==(profile==P::chip48?0x226:0x222));
        for(uint8_t opcode:{uint8_t(0x55),uint8_t(0x65)})for(unsigned x:{0u,2u,15u}) {
            CHECK(m.load({uint8_t(0xf0|x),opcode}));m.i=uint16_t(4096-x-1);
            auto start=m.i;
            for(unsigned n=0;n<=x;++n){m.v[n]=uint8_t(10+n);m.memory[start+n]=uint8_t(30+n);}
            CHECK(m.step()==S::running);
            CHECK(m.i==(profile==P::cosmac_vip?0:profile==P::chip48?4095:start));
            for(unsigned n=0;n<=x;++n)CHECK((opcode==0x55?m.memory[start+n]:m.v[n])==(opcode==0x55?10+n:30+n));
        }
        CHECK(m.load({0xf2,0x55}));m.i=4095;m.memory[4095]=77;
        CHECK(m.step()==S::bounds);CHECK(m.memory[4095]==77&&m.pc==0x200&&m.i==4095);
        CHECK(m.load({0xd0,0x12,0xd0,0x12}));m.i=0x300;m.memory[m.i]=0xff;m.memory[m.i+1]=0xff;
        m.v[0]=127;m.v[1]=63;m.v[15]=9;
        if(profile==P::cosmac_vip) {
            m.tick_60hz();CHECK(m.step()==S::waiting_vblank);CHECK(m.v[15]==9&&m.display[2047]==0);
            CHECK(m.step()==S::waiting_vblank);m.tick_60hz();
        }
        CHECK(m.step()==S::running);CHECK(m.display[2047]==1);
        CHECK(m.display[0]==(profile==P::legacy?1:0));
        if(profile==P::cosmac_vip){CHECK(m.step()==S::waiting_vblank);m.tick_60hz();}
        CHECK(m.step()==S::running);CHECK(m.display[2047]==0&&m.v[15]==1);
    }
    C m;CHECK(m.load({0xf0,0x0a}));m.keys[3]=true;
    CHECK(m.step()==S::running&&m.v[0]==3); // Default remains held-key acceptance.
    m.quirks.key_wait=K::release;CHECK(m.load({0xf1,0x0a,0xf2,0x0a}));
    m.keys[3]=true;m.keys[7]=true;m.v[1]=99;m.delay=2;m.sound=1;
    CHECK(m.step()==S::waiting);m.tick_60hz();CHECK(m.delay==1&&m.sound==0&&m.v[1]==99);
    m.keys[7]=false;CHECK(m.step()==S::waiting);m.keys[3]=false;
    CHECK(m.step()==S::running&&m.v[1]==3&&m.pc==0x202);
    CHECK(m.step()==S::waiting);m.keys[5]=true;CHECK(m.step()==S::waiting);
    m.keys[5]=false;CHECK(m.step()==S::running&&m.v[2]==5);
    m.quirks.key_wait=K::press;CHECK(m.load({0xf0,0x0a}));m.keys[2]=true;
    CHECK(m.step()==S::waiting);CHECK(m.step()==S::waiting);m.keys[2]=false;
    CHECK(m.step()==S::waiting);m.keys[2]=true;CHECK(m.step()==S::running&&m.v[0]==2);
    // Reload discards pending input; changing the PC/opcode cancels it too.
    m.quirks.key_wait=K::release;CHECK(m.load({0xf0,0x0a}));m.keys[1]=true;CHECK(m.step()==S::waiting);
    CHECK(!m.load(std::vector<uint8_t>(4097)));m.keys[1]=false;CHECK(m.step()==S::running&&m.v[0]==1);
    CHECK(m.load({0xf0,0x0a}));CHECK(m.step()==S::waiting);
    m.keys[4]=true;CHECK(m.step()==S::waiting);m.memory[0x201]=0x07;m.delay=6;
    CHECK(m.step()==S::running&&m.v[0]==6);
    CHECK(m.load({0xf0,0x0a,0xf1,0x0a}));m.keys[4]=true;CHECK(m.step()==S::waiting);
    m.keys[4]=false;m.pc=0x202;CHECK(m.step()==S::waiting&&m.v[1]==0);
    m.keys[2]=true;CHECK(m.step()==S::waiting);
    CHECK(m.load({0xf1,0x0a}));CHECK(m.step()==S::waiting&&m.pc==0x200);
    C vip(P::cosmac_vip);CHECK(vip.load({0xd0,0x11}));vip.i=0x300;vip.memory[0x300]=0x80;
    CHECK(vip.step()==S::waiting_vblank);vip.tick_60hz();
    CHECK(vip.load({0xd0,0x11}));vip.i=0x300;vip.memory[0x300]=0x80;
    CHECK(vip.step()==S::waiting_vblank&&vip.display[0]==0);
    vip.memory[0x200]=0x60;vip.memory[0x201]=42;CHECK(vip.step()==S::running&&vip.v[0]==42);
    CHECK(m.load({0x00,0xee}));m.sp=255;CHECK(m.step()==S::bounds&&m.sp==255);
    CHECK(m.load({0x00,0xff}));CHECK(m.step()==S::invalid_opcode); // No SCHIP fallback.
}
static void atomic_files() {
    const std::string path="round2-atomic-output.tmp";
    const std::string sentinel="previous artifact";
    mvm_error e{};
    CHECK(mvm_write_file(path.c_str(),sentinel.data(),sentinel.size(),&e)==MVM_OK);
    auto module=metavm::Module::compile(".function main 0 1\nconst r0 42\nret r0\n.end\n");
    for(const auto*bad:{"int","main","_private","mvm_int","bad-name"}) {
        CHECK(vmwv_emit_c_file(module.get(),bad,path.c_str(),&e)==MVM_INVALID);
        std::ifstream in(path);std::string text((std::istreambuf_iterator<char>(in)),{});CHECK(text==sentinel);
    }
    // Collision with a preexisting staging sibling must not overwrite it.
    auto sibling=path+".metavm-tmp-0";
    CHECK(mvm_write_file(sibling.c_str(),sentinel.data(),sentinel.size(),&e)==MVM_OK);
    CHECK(vmwv_emit_c_file(module.get(),"round2_factory",path.c_str(),&e)==MVM_OK);
    {std::ifstream in(sibling);std::string text((std::istreambuf_iterator<char>(in)),{});CHECK(text==sentinel);}
    CHECK(mvm_write_file_atomic("nonexistent-round2-directory/output",nullptr,0,&e)==MVM_IO);
    CHECK(mvm_write_file_atomic(nullptr,nullptr,0,&e)==MVM_INVALID);
    module.save_atomic(path);
    auto loaded=metavm::Module::load(path);metavm::VM vm;CHECK(vm.run(loaded).as.integer==42);
    CHECK(std::remove(path.c_str())==0);CHECK(std::remove(sibling.c_str())==0);
}
int main(){try{definitions();binding_transactions();ssa_text();ssa_extensions();ssa_analysis();builder_validation();chip8_profiles();atomic_files();std::cout<<"Definition lowering, registry transactions, SSA analysis and CHIP-8 refinement regressions passed\n";}
 catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
