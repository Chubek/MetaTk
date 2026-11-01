#include "MetaMLAdapter.hpp"
#include <iostream>
static std::string read_source(const char*path,size_t limit) {
    mvm_buffer buffer={};mvm_error error{};
    metavm::check(mvm_read_file(path,limit,&buffer,&error),error);
    try {
        std::string result;
        if(buffer.size)result.assign(reinterpret_cast<const char*>(buffer.data),buffer.size);
        mvm_buffer_dispose(&buffer);return result;
    }catch(...){mvm_buffer_dispose(&buffer);throw;}
}
int main(int argc,char**argv) {
    try {
        constexpr size_t limit=16*1024*1024;
        if(argc==5&&std::string(argv[1])=="--definition") {
            auto definition=vmweave::compile_definition(read_source(argv[2],limit),limit,argv[2]);
            auto bytes=read_source(argv[3],65536);
            auto module=definition.lower(std::vector<uint8_t>(bytes.begin(),bytes.end()));
            module.save_atomic(argv[4]);return 0;
        }
        if(argc!=3) { std::cerr<<"usage: vmweave-metaml INPUT.vmweave OUTPUT.mvmb\n"
            "       vmweave-metaml --definition INPUT.vmweave GUEST.bin OUTPUT.mvmb\n"; return 2; }
        auto module=vmweave::compile(read_source(argv[1],limit),limit,argv[1]);
        module.save_atomic(argv[2]);return 0;
    } catch(const std::exception&e) { std::cerr<<e.what()<<'\n'; return 1; }
}
