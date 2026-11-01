#include "VMLibrary/SSAVM/ISSA.hpp"
#include "MetaVM.h"
#include <charconv>
#include <iostream>
int main(int argc,char**argv) {
    using I=metavm::machines::ISSA;I::Limits limits;I::AnalysisLimits analysis_limits;
    int file=1;bool check=false;
    try {
        while(file<argc&&std::string(argv[file]).compare(0,2,"--")==0) {
            std::string option=argv[file++];if(option=="--")break;
            if(option=="--check"){check=true;continue;}
            if(file==argc)throw std::runtime_error("missing limit value");
            std::string text=argv[file++];uint64_t value=0;
            auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
            if(parsed.ec!=std::errc()||parsed.ptr!=text.data()+text.size())throw std::runtime_error("invalid unsigned limit");
            if(option=="--max-steps")limits.max_steps=value;
            else if(option=="--max-analysis-work")analysis_limits.max_work=value;
            else {
                if(value>SIZE_MAX)throw std::runtime_error("limit exceeds host size range");
                if(option=="--max-array-elements")limits.max_array_elements=size_t(value);
                else if(option=="--max-total-elements")limits.max_allocated_elements=size_t(value);
                else if(option=="--max-arrays")limits.max_array_allocations=size_t(value);
                else if(option=="--max-analysis-states")analysis_limits.max_states=size_t(value);
                else if(option=="--max-analysis-cells")analysis_limits.max_cells=size_t(value);
                else throw std::runtime_error("unknown option: "+option);
            }
        }
        if(file==argc)throw std::runtime_error("missing input file");
        if(check&&file+1!=argc)throw std::runtime_error("--check does not accept execution arguments");
    }catch(const std::exception&e){
        std::cerr<<e.what()<<"\nusage: ssavm-run [--max-steps N] [--max-array-elements N]\n"
            "                 [--max-total-elements N] [--max-arrays N] [--check]\n"
            "                 [--max-analysis-states N] [--max-analysis-cells N] [--max-analysis-work N]\n"
            "                 [--] FILE.ssa [INTEGER_ARGUMENT ...]\n";return 2;
    }
    mvm_buffer buffer={};mvm_error error{};
    try {
        if(mvm_read_file(argv[file],limits.max_source_bytes,&buffer,&error)!=MVM_OK)throw std::runtime_error(error.message);
        std::string source;if(buffer.size)source.assign(reinterpret_cast<const char*>(buffer.data),buffer.size);
        mvm_buffer_dispose(&buffer);
        std::vector<int64_t>args;
        for(int k=file+1;k<argc;++k) {
            std::string text=argv[k];int64_t value;
            auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
            if(parsed.ec!=std::errc()||parsed.ptr!=text.data()+text.size())throw std::runtime_error("invalid integer argument");
            args.push_back(value);
        }
        auto program=check?I::parse_checked(source,limits,analysis_limits):I::parse(source,limits);
        if(check){std::cout<<"verified\n";return 0;}
        auto result=I::run_limited(program,limits,args);
        if(std::holds_alternative<int64_t>(result.value))std::cout<<std::get<int64_t>(result.value)<<'\n';
        else std::cout<<"array["<<std::get<metavm::machines::ISSA::Array>(result.value)->size()<<"]\n";
        return 0;
    }catch(const std::exception&e){mvm_buffer_dispose(&buffer);std::cerr<<argv[file]<<": "<<e.what()<<'\n';return 1;}
}
