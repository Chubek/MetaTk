// A small language front end: % directives, $(macros), and a custom %say.
// Run without arguments for a demonstration, or pass a UTF-8 source file.
#include <GPPX.hpp>
#include <GPPX-all.h>
#include <iostream>
#include <memory>

int main(int argc,char **argv) {
    if (argc>2) {
        std::cerr << "usage: " << argv[0] << " [source.my]\n";
        return 2;
    }
    try {
        gppx::Engine engine;
        engine.set("profile","percent");
        const GPPX_Style style={"dollar",";","/*","*/",0};
        if (gppx_style_apply(engine.state(),&style) ||
            gppx_metasym_define(engine.state(),"lang","version","1"))
            throw gppx::Error("cannot configure mylang");
        engine.registerDirective("say",[](std::string_view args) {
            // Produce a quoted literal; directive callback arguments are raw.
            std::string out="print(\"";
            for (char c:args) {
                if (c=='\\' || c=='\"') out+='\\';
                out+=c;
            }
            return out+"\");\n";
        });
        if (argc==2) {
            char *raw=nullptr;
            if (gppx_utf8_load(argv[1],&raw))
                throw gppx::Error("cannot read source as UTF-8 text");
            std::unique_ptr<char,decltype(&gppx_free)> source(raw,gppx_free);
            std::cout << engine.process(source.get(),argv[1]);
        } else {
            std::cout << engine.process(
                "%define WHO world\n"
                "%say Hello from mylang\n"
                "version = $(lang_version);\n"
                "greet($(WHO));\n","<mylang-demo>");
        }
        return std::cout ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "mylang: " << e.what() << '\n';
        return 1;
    }
}
