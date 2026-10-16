#include "MetaChain.hpp"
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    if (argc < 2 || std::string_view(argv[1]) == "--help") {
        std::cout << "MetaChain " << metachain::version << "\n"
            "usage: metachain COMMAND [INPUT|-] [-o OUTPUT] [--no-optimize] [--max-steps N]\n"
            "commands: parse rewrite check run optimize encode decode native build assemble object link-assembly\n"
            "          link OBJECT... -o OUTPUT\n";
        return argc < 2 ? 2 : 0;
    }
    const std::string command = argv[1];
    std::string output;
    std::vector<std::string> inputs;
    metachain::Options options;
    for (int n=2; n<argc; ++n) {
        if (std::string_view(argv[n])=="-o") {
            if (++n==argc || !output.empty()) { std::cerr << "invalid -o option\n"; return 2; }
            output=argv[n];
        } else if (std::string_view(argv[n])=="--no-optimize") options.optimize=false;
        else if (std::string_view(argv[n])=="--max-steps") {
            if (++n==argc) { std::cerr << "--max-steps requires an integer\n"; return 2; }
            auto v=metachain::text::integer(argv[n]);
            if (!v || v->negative || v->magnitude>std::numeric_limits<std::size_t>::max()) { std::cerr << "invalid step limit\n"; return 2; }
            options.vm.max_steps=static_cast<std::size_t>(v->magnitude);
        } else if (std::string_view(argv[n]).starts_with('-') && std::string_view(argv[n])!="-") { std::cerr << "unknown option\n"; return 2; }
        else inputs.emplace_back(argv[n]);
    }
    if (command=="link") {
        auto r=ld_tk::Linker().link_files(inputs);
        if (!r) { for (const auto& e:r.diagnostics) std::cerr << e.object << ": " << e.message << '\n'; return 1; }
        if (output.empty()) { std::cerr << "link requires -o OUTPUT\n"; return 2; }
        if (!ld_tk::Linker::write_file(output,r.image)) { std::cerr << "cannot write " << output << '\n'; return 1; }
        return 0;
    }
    if (inputs.size()>1) { std::cerr << "command accepts one input\n"; return 2; }
    std::ifstream file;
    std::istream* in=&std::cin;
    if (!inputs.empty() && inputs[0]!="-") {
        file.open(inputs[0],std::ios::binary);
        if (!file) { std::cerr << "cannot open " << inputs[0] << '\n'; return 1; }
        in=&file;
    }
    std::string source;
    char buffer[8192];
    while (in->read(buffer,sizeof(buffer)) || in->gcount()) {
        if (source.size()+static_cast<std::size_t>(in->gcount())>64u*1024u*1024u) { std::cerr << "input size limit exceeded\n"; return 1; }
        source.append(buffer,static_cast<std::size_t>(in->gcount()));
    }
    if (!in->eof()) { std::cerr << "input read failed\n"; return 1; }
    metachain::TextResult r;
    if (command=="decode") {
        auto p=metachain::Toolchain().decode(std::span(reinterpret_cast<const std::uint8_t*>(source.data()),source.size()));
        r.diagnostics=std::move(p.diagnostics); if (p.value) r.text=metail::emit(*p);
    } else r=metachain::evaluate(command,source,options);
    if (!r) { for (const auto& e:r.diagnostics) std::cerr << metachain::format(e) << '\n'; return 1; }
    const bool binary=command=="encode" || command=="build" || command=="assemble" || command=="object" || command=="link-assembly";
    if (binary && output.empty()) { std::cerr << "binary commands require -o OUTPUT\n"; return 2; }
    if (!output.empty()) {
        std::vector<std::uint8_t> bytes=binary ? std::move(r.bytes) : std::vector<std::uint8_t>(r.text.begin(),r.text.end());
        if (!ld_tk::Linker::write_file(output,bytes)) { std::cerr << "cannot write " << output << '\n'; return 1; }
    } else { std::cout << r.text; if (!std::cout) return 1; }
    return 0;
}
