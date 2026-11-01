#include "MetaTeXAdapter.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char**argv) {
    if(argc!=2) {std::cerr<<"usage: vmdesc FILE.vmdesc\n";return 2;}
    std::ifstream file(argv[1]);if(!file)return 1;
    std::string source((std::istreambuf_iterator<char>(file)),{});
    metatex::Engine engine;vmdesc::install(engine);
    auto doc=engine.compile(source,argv[1]);
    for(const auto&d:doc.diagnostics)if(d.severity==metatex::Diagnostic::Severity::Error){std::cerr<<d.message<<'\n';return 1;}
    std::cout<<doc.title<<'\n';for(const auto&b:doc.blocks){for(const auto&s:b.spans)std::cout<<s.text;std::cout<<'\n';}
}
