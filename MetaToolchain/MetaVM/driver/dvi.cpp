#include "VMLibrary/DVI/DviMachine.hpp"
#include "vmweave/DviReplay.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

using D=metavm::machines::DviMachine;
static std::string hex(const std::string&bytes) {
    static const char digits[]="0123456789abcdef";
    std::string out;
    for(unsigned char c:bytes){out+=digits[c>>4];out+=digits[c&15];}
    return out;
}
int main(int argc,char**argv) {
    std::string input,metrics_file;auto engine=MVM_ENGINE_DEFAULT;
    for(int i=1;i<argc;++i) {
        std::string a=argv[i];
        if(a=="--metrics"&&i+1<argc)metrics_file=argv[++i];
        else if(a=="--threaded")engine=MVM_ENGINE_THREADED;
        else if(a=="--bytecode")engine=MVM_ENGINE_BYTECODE;
        else if(a=="--xvm")engine=MVM_ENGINE_XVM;
        else if(a=="--help") {
            std::cout<<"Usage: dvi-run [--metrics FONT_WIDTHS.tsv] [--xvm|--bytecode|--threaded] FILE.dvi\n"
                "Metrics: decimal FONT CHARACTER WIDTH (scaled DVI units), one row per character.\n";
            return 0;
        } else if(a.empty()||a[0]=='-'||!input.empty()) {
            std::cerr<<"Usage: dvi-run [--metrics FONT_WIDTHS.tsv] [--xvm|--bytecode|--threaded] FILE.dvi\n";return 2;
        } else input=a;
    }
    if(input.empty()){std::cerr<<"DVI input path required; use --help\n";return 2;}
    try {
        std::map<std::pair<int64_t,int64_t>,int32_t> widths;
        if(!metrics_file.empty()) {
            std::ifstream in(metrics_file);
            if(!in){std::cerr<<"Cannot open metrics file\n";return 3;}
            char record[1024];size_t number=0;
            while(in.getline(record,sizeof(record))) {
                std::string line(record);
                ++number;if(number>1000000)throw std::runtime_error("metrics row limit");
                auto comment=line.find('#');if(comment!=std::string::npos)line.resize(comment);
                std::istringstream row(line);row>>std::ws;if(row.eof())continue;
                int64_t font,character,width;std::string extra;
                if(!(row>>font>>character>>width)||(row>>extra)||width<INT32_MIN||width>INT32_MAX)
                    throw std::runtime_error("invalid metrics row "+std::to_string(number));
                if(!widths.emplace(std::make_pair(font,character),int32_t(width)).second)
                    throw std::runtime_error("duplicate metrics row "+std::to_string(number));
            }
            if(in.bad()){std::cerr<<"Cannot read metrics file\n";return 3;}
            if(!in.eof())throw std::runtime_error("metrics row exceeds 1023 bytes");
        }
        D::Limits limits;std::ifstream in(input,std::ios::binary);
        if(!in){std::cerr<<"Cannot open DVI file\n";return 3;}
        std::vector<uint8_t> bytes;char block[8192];
        while(in.read(block,sizeof(block))||in.gcount()) {
            size_t n=size_t(in.gcount());
            if(n>limits.max_input_bytes-bytes.size())throw std::runtime_error("DVI input byte limit");
            bytes.insert(bytes.end(),block,block+n);
        }
        if(!in.eof()){std::cerr<<"Cannot read DVI file\n";return 3;}
        auto document=D::run(bytes,[&](const D::Font&font,int64_t character) {
            auto found=widths.find({font.number,character});
            if(found==widths.end())throw std::runtime_error("missing metric for font "+
                std::to_string(font.number)+" character "+std::to_string(character));
            return found->second;
        },limits);
        vmweave::DviReplay replay(std::move(document),[](const D::Event&e) {
            const char*kind=e.kind==D::Kind::begin_page?"bop":e.kind==D::Kind::end_page?"eop":
                e.kind==D::Kind::glyph?"glyph":e.kind==D::Kind::rule?"rule":"special";
            std::cout<<e.source_offset<<' '<<kind<<" page="<<e.page<<" h="<<e.position.h<<" v="<<e.position.v;
            if(e.kind==D::Kind::glyph)std::cout<<" font="<<e.font<<" char="<<e.character<<" advance="<<(e.advances?e.width:0);
            if(e.kind==D::Kind::rule)std::cout<<" height="<<e.height<<" width="<<e.width;
            if(e.kind==D::Kind::special)std::cout<<" bytes="<<hex(e.bytes);
            std::cout<<'\n';
        });
        auto module=replay.module();metavm::VM vm;mvm_error error{};
        metavm::check(replay.bind(vm.get(),&error),error);
        vm.run(module,0,{},engine);
        if(!std::cout){std::cerr<<"Cannot write DVI trace\n";return 3;}
        return 0;
    } catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
