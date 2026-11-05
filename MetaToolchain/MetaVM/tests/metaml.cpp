#include "vmweave/MetaMLAdapter.hpp"
#include "vmweave/examples/ToyR16.hpp"
#include "VMLibrary/DRMVM/Graphics.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#define CHECK(x) do{if(!(x))throw std::runtime_error("check failed: " #x);}while(0)
static std::string read(const std::filesystem::path&path) {
    std::ifstream file(path,std::ios::binary);CHECK(file.good());
    return std::string(std::istreambuf_iterator<char>(file),{});
}
static std::vector<uint8_t> bytes(const metavm::Module&module) {
    mvm_buffer buffer={};mvm_error error{};
    metavm::check(mvm_serialize(module.get(),&buffer,&error),error);
    try {
        std::vector<uint8_t> result(buffer.data,buffer.data+buffer.size);
        mvm_buffer_dispose(&buffer);return result;
    }catch(...){mvm_buffer_dispose(&buffer);throw;}
}
static void traps(const std::function<void()>&f,const std::string&message) {
    try{f();}catch(const std::exception&e){
        if(std::string(e.what()).find(message)==std::string::npos)
            throw std::runtime_error("expected '"+message+"', received: "+e.what());
        return;
    }
    throw std::runtime_error("expected failure: "+message);
}
int main(int argc,char**argv) {
    try {
        CHECK(argc==2);std::filesystem::path root=argv[1];size_t count=0;
        for(const auto&dir:std::filesystem::directory_iterator(root/"VMLibrary"))if(dir.is_directory()) {
            auto base=dir.path()/dir.path().filename();auto source=base;source+=".vmweave";
            auto assembly=base;assembly+=".mvm";
            auto module=vmweave::compile(read(source),16*1024*1024,source.string());
            CHECK(bytes(module)==bytes(metavm::Module::compile(read(assembly))));
            for(auto engine:{MVM_ENGINE_BYTECODE,MVM_ENGINE_THREADED,MVM_ENGINE_XVM}) {
                metavm::machines::Graphics surface(4,4);metavm::VM vm;mvm_error error{};
                if(dir.path().filename()=="DRMVM")CHECK(surface.bind(vm.get(),&error)==MVM_OK);
                CHECK(vm.run(module,0,{},engine).as.integer==42);
                if(dir.path().filename()=="DRMVM")CHECK(surface.pixels()[5]==0xff0000);
            }
            ++count;
        }
        CHECK(count==49);
        auto factorial=vmweave::compile(read(root/"vmweave/factorial.vmweave"));
        CHECK(bytes(factorial)==bytes(metavm::Module::compile(read(root/"vmweave/factorial.mvm"))));
        auto generated=vmweave::compile(
            "val emit_loop := fn name -> do { vm_label name; vm_binary \"mul\" 0 0 1; "
            "vm_binary \"sub\" 1 1 2; vm_binary \"eq\" 4 1 3; vm_jump_false 4 name }\n"
            "val _ := vm_function \"main\" 0 5\nval _ := vm_const 0 1\nval _ := vm_const 1 10\n"
            "val _ := vm_const 2 1\nval _ := vm_const 3 0\nval _ := emit_loop \"loop\"\n"
            "val _ := vm_return 0\nval _ := vm_end ()\n");
        CHECK(bytes(generated)==bytes(factorial));
        for(auto engine:{MVM_ENGINE_BYTECODE,MVM_ENGINE_THREADED,MVM_ENGINE_XVM}) {
            metavm::VM vm;CHECK(vm.run(factorial,0,{},engine).as.integer==3628800);
        }
        const auto toy_source=read(root/"vmweave/examples/ToyR16.vmweave");
        auto definition=vmweave::compile_definition(toy_source);
        for(const std::vector<uint8_t>&code:std::vector<std::vector<uint8_t>>{
            {0x10,40,0x11,2,0x20,0x10,0x30,3,0x50,0},
            {0x10,3,0x11,1,0x60,0x10,0x40,4,0x50,0}}) {
            CHECK(bytes(definition.lower(code))==bytes(vmweave::examples::toy_r16().lower(code)));
            for(auto engine:{MVM_ENGINE_BYTECODE,MVM_ENGINE_THREADED,MVM_ENGINE_XVM}) {
                auto actual=definition.execute(code,{},{},mvm_default_limits(),engine);
                auto expected=vmweave::examples::toy_r16().execute(code,{},{},mvm_default_limits(),engine);
                CHECK(actual.value==expected.value&&actual.registers==expected.registers&&actual.memory==expected.memory);
            }
        }
        const std::string head="val _ := vd_machine \"test\" 8 \"little\" 2 8 1\nval _ := vd_rule \"rule\" 0 0\n";
        // Exercise every binary primitive, including normalized comparisons.
        for(const auto&op:{"add","sub","mul","div","mod","and","or","xor","shl","shr","eq","lt"}) {
            auto source=head+"val a := vd_const 7\nval b := vd_field 0 8 false\nval c := vd_binary \""+op+
                "\" a b\nval _ := vd_halt c\nval _ := vd_end ()\n";
            auto d=vmweave::compile_definition(source);int64_t expected=0;
            mvm_opcode opcode=MVM_OP_ADD;
            for(int k=0;k<MVM_OP_COUNT;++k)if(std::string(mvm_opcode_name(mvm_opcode(k)))==op)opcode=mvm_opcode(k);
            CHECK(mvm_integer_eval(opcode,7,2,&expected)==MVM_OK);
            for(auto engine:{MVM_ENGINE_BYTECODE,MVM_ENGINE_THREADED,MVM_ENGINE_XVM})CHECK(d.execute({2},{},{},mvm_default_limits(),engine).value==expected);
        }
        auto memory=vmweave::compile_definition(head+
            "val index := vd_const 0\nval value := vd_field 0 8 true\n"
            "val _ := vd_write_byte index value\nval result := vd_read_byte index\n"
            "val _ := vd_halt result\nval _ := vd_end ()\n");
        CHECK(memory.execute({255}).value==255);
        auto jump=vmweave::compile_definition(
            "val _ := vd_machine \"jump\" 8 \"big\" 1 64 0\n"
            "val _ := vd_rule \"jump\" 255 0\nval target := vd_const 1\nval _ := vd_jump target\nval _ := vd_end ()\n"
            "val _ := vd_rule \"halt\" 255 1\nval result := vd_const 42\nval _ := vd_halt result\nval _ := vd_end ()\n");
        CHECK(jump.execute({0,1}).value==42);
        traps([&]{vmweave::compile_definition(head+"val _ := vd_halt 0\n");},"invalid value reference");
        traps([&]{vmweave::compile_definition(head+"val x := vd_field 0 0 false\n");},"invalid definition field");
        traps([&]{vmweave::compile_definition(head+"val x := vd_field (-1) 8 false\n");},"argument range");
        std::string too_many=head;
        for(size_t n=0;n<4097;++n)too_many+="val _ := vd_const 0\n";
        traps([&]{vmweave::compile_definition(too_many);},"node limit");
        auto dynamic=vmweave::compile_definition(head+"val index := vd_const 0\nval target := vd_read_register index\nval _ := vd_jump target\nval _ := vd_end ()\n");
        traps([&]{dynamic.lower({0});},"dynamic branch target");
        traps([&]{vmweave::compile_definition(head+"val x := vd_const 0\nval _ := vd_halt x\nval y := vd_const 1\n");},"after control transfer");
        traps([&]{vmweave::compile_definition(head+"val x := vd_const 0\n");},"unterminated");
        traps([&]{vmweave::compile_definition(toy_source+"val _ := vd_rule \"overlap\" 0 0\nval x := vd_const 0\nval _ := vd_halt x\nval _ := vd_end ()\n");},"ambiguous");
        traps([&]{definition.lower({0xff,0xff});},"unknown guest opcode");
        traps([&]{vmweave::compile("12345",4);},"source limit");
        traps([&]{vmweave::compile(std::string("a\0b",3));},"NUL");
        traps([&]{vmweave::compile("val _ := let rec loop := fn x -> loop x in loop 0\n",1024,"budget.vmweave",{100,256});},"M0410");
        traps([&]{vmweave::compile("val _ := let rec loop := fn x -> 1 + loop x in loop 0\n",1024,"depth.vmweave",{100000,16});},"depth");
        try{vmweave::compile("\nval x := missing\n",1024,"error.vmweave");CHECK(false);}
        catch(const vmweave::MetaMLError&e) {
            CHECK(!e.diagnostics.empty());const auto&d=e.diagnostics[0];
            CHECK(d.code=="M0204"&&d.primary_span.line==2&&d.primary_span.file=="error.vmweave");
            CHECK(std::string(e.what()).find("val x := missing")!=std::string::npos);
        }
        // Also compile the public optional header alone in this translation unit.
        std::cout<<"49 real MetaML catalog translations, definitions, diagnostics and evaluation limits passed\n";
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
