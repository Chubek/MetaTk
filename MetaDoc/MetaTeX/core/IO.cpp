#include "MetaTeX.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace metatex {
std::string readFile(const std::filesystem::path& path) {
    return readFile(path,16*1024*1024);
}
std::string readFile(const std::filesystem::path& path,std::size_t maxBytes) {
    std::ifstream f(path, std::ios::binary);
    if(!f) throw std::runtime_error("cannot open input: "+path.string());
    std::string bytes;char buffer[8192];
    while(f.read(buffer,sizeof(buffer)) || f.gcount()){
        auto count=static_cast<std::size_t>(f.gcount());
        if(count>maxBytes-bytes.size())throw std::runtime_error("input size limit exceeded: "+path.string());
        bytes.append(buffer,count);
    }
    if(f.bad())throw std::runtime_error("read failed: "+path.string());
    return bytes;
}
void writeFile(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream f(path,std::ios::binary);
    if(!f) throw std::runtime_error("cannot open output: "+path.string());
    f.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    f.close();
    if(!f) throw std::runtime_error("write failed: "+path.string());
}
}
