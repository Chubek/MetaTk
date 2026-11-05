#include "catalog_factories.hpp"
#include "VMLibrary/Library.hpp"
#include "VMLibrary/DRMVM/Graphics.hpp"
#include "Machinery/MetaVM.hpp"
#include "vmweave/vmweave.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(x) do{if(!(x))throw std::runtime_error("check failed: " #x);}while(0)
static std::vector<uint8_t> binary(const mvm_module*module) {
    mvm_buffer buffer{};mvm_error error{};
    metavm::check(mvm_serialize(module,&buffer,&error),error);
    try {
        std::vector<uint8_t> result(buffer.data,buffer.data+buffer.size);
        mvm_buffer_dispose(&buffer);return result;
    } catch(...) {mvm_buffer_dispose(&buffer);throw;}
}
static void execute(const mvm_module*module) {
    for(auto engine:{MVM_ENGINE_BYTECODE,MVM_ENGINE_THREADED,MVM_ENGINE_XVM}) {
        metavm::machines::Graphics surface(4,4);metavm::VM vm;mvm_error error{};
        metavm::check(surface.bind(vm.get(),&error),error);
        mvm_value result{};
        metavm::check(mvm_run(vm.get(),module,0,nullptr,0,engine,&result,&error),error);
        CHECK(result.kind==MVM_INT&&result.as.integer==42);
    }
}
int main(int argc,char**argv) {
    try {
        CHECK(argc==3);
        CHECK(sizeof generated_examples/sizeof *generated_examples==metavm::library::catalog().size());
        for(const auto&example:generated_examples) {
            auto*profile=metavm::library::find(example.name);CHECK(profile);
            mvm_error error{};mvm_module*raw=nullptr;
            metavm::check(example.factory(&raw,&error),error);
            std::unique_ptr<mvm_module,decltype(&mvm_module_destroy)> emitted(raw,mvm_module_destroy);
            raw=nullptr;metavm::check(profile->example_module(&raw,&error),error);
            std::unique_ptr<mvm_module,decltype(&mvm_module_destroy)> catalog(raw,mvm_module_destroy);
            const auto base=std::string("/VMLibrary/")+example.name+"/"+example.name;
            auto companion=metavm::Module::load(std::string(argv[1])+base+".mvm");
            auto loaded=metavm::Module::load(std::string(argv[2])+base+".mvmb");
            auto expected=binary(companion.get());
            CHECK(binary(emitted.get())==expected&&binary(catalog.get())==expected&&binary(loaded.get())==expected);
            execute(emitted.get());execute(loaded.get());
            // Independently exercise stream emission as well as the atomic-file
            // path used by the CLI, then compare with the compiled C artifact.
            FILE*stream=std::tmpfile();CHECK(stream);
            const std::string symbol="generated_"+std::string(example.name)+"_example_module";
            auto identifier=symbol;
            for(char&c:identifier)if(c=='-')c='_';
            auto status=vmwv_emit_c(companion.get(),identifier.c_str(),stream,&error);
            if(status!=MVM_OK){std::fclose(stream);metavm::check(status,error);}
            CHECK(std::fflush(stream)==0);CHECK(std::fseek(stream,0,SEEK_SET)==0);
            std::string text;char block[4096];size_t count;
            while((count=std::fread(block,1,sizeof block,stream)))text.append(block,count);
            const bool failed=std::ferror(stream);CHECK(std::fclose(stream)==0);CHECK(!failed);
            std::ifstream file(std::string(argv[2])+base+".c",std::ios::binary);CHECK(file);
            const std::string stored((std::istreambuf_iterator<char>(file)),{});CHECK(stored==text);
            raw=reinterpret_cast<mvm_module*>(uintptr_t(1));
            CHECK(example.factory(nullptr,&error)==MVM_INVALID);
            // Each call returns an independent owned module.
            metavm::check(example.factory(&raw,&error),error);CHECK(raw!=emitted.get());mvm_module_destroy(raw);
        }
        std::cout<<"49 generated C factories, binary/assembly/catalog parity and both engines passed\n";
    } catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
