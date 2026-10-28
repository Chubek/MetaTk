#include "vmweave/Weave.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
struct IO : std::runtime_error { using std::runtime_error::runtime_error; };
template<class T> T number(const char *text) {
    std::string_view s(text);T value{};auto r=std::from_chars(s.data(),s.data()+s.size(),value);
    if (s.empty() || r.ec!=std::errc{} || r.ptr!=s.data()+s.size()) throw std::invalid_argument("invalid integer argument");
    return value;
}
std::vector<std::uint8_t> read(const std::filesystem::path &path) {
    std::ifstream in(path,std::ios::binary);if (!in) throw IO("cannot open input: "+path.string());
    std::vector<std::uint8_t> out;char chunk[8192];constexpr auto limit=32*1024*1024;
    while (in.read(chunk,sizeof chunk) || in.gcount()) {
        auto n=static_cast<std::size_t>(in.gcount());
        if (n>limit-out.size()) throw std::length_error("input byte limit exceeded");
        out.insert(out.end(),chunk,chunk+n);
    }
    if (!in.eof()) throw IO("input read failed: "+path.string());
    return out;
}
void write(const std::filesystem::path &path,const std::vector<std::uint8_t> &bytes) {
    std::ofstream out(path,std::ios::binary);if (!out) throw IO("cannot open output: "+path.string());
    out.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()));out.close();
    if (!out) throw IO("output write failed: "+path.string());
}
}
int main(int argc,char **argv) {
    try {
        if (argc==5 && std::string_view(argv[1])=="compile") {
            const std::filesystem::path input=argv[3],output=argv[4];
            if (std::filesystem::weakly_canonical(input)==std::filesystem::weakly_canonical(output) ||
                (std::filesystem::exists(output) && std::filesystem::equivalent(input,output)))
                throw std::invalid_argument("input and output must differ");
            auto bytes=read(input);
            std::string source(bytes.begin(),bytes.end());
            auto program=metavm::weave(source,argv[2]);write(output,program.serialize());return 0;
        }
        if (argc>=4 && std::string_view(argv[1])=="run") {
            auto program=metavm::Program::deserialize(read(argv[2]));
            auto budget=number<std::uint64_t>(argv[3]);std::vector<std::int64_t> args;
            for (int i=4;i<argc;++i) args.push_back(number<std::int64_t>(argv[i]));
            auto frame=metavm::make_frame(program,args);auto stop=metavm::run(program,frame,budget);
            if (stop==metavm::Stop::Returned) { std::cout<<"result="<<frame.result<<" steps="<<frame.steps<<'\n';return 0; }
            std::cerr<<(stop==metavm::Stop::Budget?"instruction budget exhausted":"VM trap")
                <<" pc="<<frame.pc<<" trap="<<static_cast<std::uint64_t>(frame.trap)<<'\n';return 1;
        }
        std::cerr<<"usage: vmweave compile FUNCTION INPUT.mi OUTPUT.mvm\n"
                   "       vmweave run INPUT.mvm BUDGET [INT64 ...]\n";return 2;
    } catch(const IO &e) { std::cerr<<e.what()<<'\n';return 3; }
    catch(const std::filesystem::filesystem_error &e) { std::cerr<<e.what()<<'\n';return 3; }
    catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
