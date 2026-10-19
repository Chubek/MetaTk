#include "MetaAS.hpp"
#include "Internal.hpp"
#include <charconv>
#include <fstream>
#include <iostream>

namespace {
void usage() {
    std::cout << "Usage: metaas --spec ARCH [OPTIONS] INPUT\n"
                 "       metaas --spec ARCH --list-encoders\n\n"
                 "  --spec FILE        MetaArch .march / YAML / JSON / XML / S-expression spec\n"
                 "  --bind ENC=P1,P2   explicit source operand order (repeatable)\n"
                 "  --target N         zero-based target index (default 0)\n"
                 "  --origin N         flat-image base address (default 0)\n"
                 "  --format FORMAT    bin (default), hex, or json (MetaAS relocatable object)\n"
                 "  --max-source N     source byte limit (default 16777216)\n"
                 "  --max-output N     output byte limit (default 16777216)\n"
                 "  -o FILE            output file (default stdout); INPUT - reads stdin\n"
                 "  --list-encoders    list parameter names and encoded sizes\n"
                 "  --help, --version\n";
}
std::uint64_t number(std::string_view text) {
    int base=10;
    if (text.starts_with("0x")) { base=16; text.remove_prefix(2); }
    std::uint64_t result=0;
    auto parsed=std::from_chars(text.data(),text.data()+text.size(),result,base);
    if (text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size()) throw std::invalid_argument("expected unsigned decimal or hexadecimal integer");
    return result;
}
std::size_t size_number(std::string_view text) {
    auto n=number(text); if (n>SIZE_MAX) throw std::invalid_argument("size exceeds platform range");
    return static_cast<std::size_t>(n);
}
void diagnostics(const std::vector<metaas::Diagnostic>& errors) {
    for (const auto& e:errors) {
        if (!e.file.empty()) std::cerr << e.file << ':';
        if (e.line) std::cerr << e.line << ':' << e.column << ':';
        std::cerr << " error: " << e.message << '\n';
    }
}
std::string read(std::istream& input,std::size_t limit) {
    std::string source; char chunk[8192];
    while (input) {
        input.read(chunk,sizeof chunk);
        auto got=static_cast<std::size_t>(input.gcount());
        if (got>limit-source.size()) throw std::length_error("source byte limit exceeded");
        source.append(chunk,got);
    }
    if (!input.eof() || input.bad()) throw std::runtime_error("source read failed");
    return source;
}
}
int main(int argc,char **argv) {
    metaas::Options options;
    std::string spec,input,output,format="bin"; bool list=false;
    try {
        for (int n=1;n<argc;++n) {
            const std::string arg=argv[n];
            auto value=[&]() { if (++n==argc) throw std::invalid_argument("missing value for " + arg); return std::string(argv[n]); };
            if (arg=="--help" || arg=="-h") { usage(); return 0; }
            if (arg=="--version") { std::cout << "MetaAS 0.1.0\n"; return 0; }
            if (arg=="--spec") spec=value();
            else if (arg=="--target") options.target=size_number(value());
            else if (arg=="--origin") options.origin=number(value());
            else if (arg=="--format") format=value();
            else if (arg=="--max-source") options.max_source_bytes=size_number(value());
            else if (arg=="--max-output") options.max_output_bytes=size_number(value());
            else if (arg=="-o") output=value();
            else if (arg=="--list-encoders") list=true;
            else if (arg=="--bind") {
                auto text=value(); auto equal=text.find('=');
                if (equal==text.npos || !metaas::detail::identifier(text.substr(0,equal))) throw std::invalid_argument("binding must be ENC=P1,P2");
                std::vector<std::string> names;
                std::string_view tail(text.data()+equal+1,text.size()-equal-1);
                while (!tail.empty()) {
                    auto comma=tail.find(','); auto name=metaas::detail::trim(tail.substr(0,comma));
                    if (!metaas::detail::identifier(name)) throw std::invalid_argument("invalid binding parameter");
                    names.emplace_back(name);
                    if (comma==tail.npos) break;
                    tail.remove_prefix(comma+1);
                    if (tail.empty()) throw std::invalid_argument("trailing comma in binding");
                }
                if (!options.bindings.emplace(text.substr(0,equal),std::move(names)).second) throw std::invalid_argument("duplicate encoder binding");
            } else if (arg=="--") {
                if (n+2!=argc || !input.empty()) throw std::invalid_argument("expected one input after --");
                input=argv[++n];
            } else if (arg!="-" && arg.starts_with('-')) throw std::invalid_argument("unknown option: " + arg);
            else {
                if (!input.empty()) throw std::invalid_argument("expected one assembly input");
                input=arg;
            }
        }
        if (spec.empty() || (!list && input.empty()) || (list && (!input.empty() || !output.empty()))) throw std::invalid_argument("supply --spec and one INPUT, or --list-encoders");
        if (format!="bin" && format!="hex" && format!="json") throw std::invalid_argument("unknown output format: " + format);
    } catch (const std::exception& e) { std::cerr << "metaas: " << e.what() << '\n'; return 2; }
    try {
        // Distinguish a missing/unreadable architecture file from invalid schema.
        { std::ifstream check(spec,std::ios::binary); if (!check) { std::cerr << "metaas: cannot open " << spec << '\n'; return 3; } }
        auto loaded=metaas::Assembler::load(spec);
        if (!loaded) { diagnostics(loaded.diagnostics); return 1; }
        if (list) {
            for (const auto& e:loaded.value->encoders()) {
                std::cout << e.name << " (";
                for (std::size_t n=0;n<e.parameters.size();++n) std::cout << (n?",":"") << e.parameters[n];
                std::cout << ") " << e.size << " bytes\n";
            }
            return std::cout ? 0 : 3;
        }
        std::string source;
        if (input=="-") source=read(std::cin,options.max_source_bytes);
        else {
            std::ifstream stream(input,std::ios::binary);
            if (!stream) { std::cerr << "metaas: cannot open " << input << '\n'; return 3; }
            source=read(stream,options.max_source_bytes);
        }
        auto assembled=loaded.value->assemble(source,options,input=="-"?"<stdin>":input);
        if (!assembled) { diagnostics(assembled.diagnostics); return 1; }
        std::string text; std::vector<std::uint8_t> bytes;
        if (format=="json") text=assembled.value->json();
        else {
            auto flat=assembled.value->binary(options.max_output_bytes);
            if (!flat) { diagnostics(flat.diagnostics); return 1; }
            bytes=std::move(*flat.value);
            if (format=="hex") {
                auto hex=metaas::detail::hex(bytes);
                for (std::size_t n=0;n<hex.size();n+=2) { if (n) text+=' '; text.append(hex,n,2); }
                text+='\n';
            }
        }
        std::ofstream file;
        std::ostream *sink=&std::cout;
        if (!output.empty() && output!="-") {
            file.open(output,std::ios::binary|std::ios::trunc);
            if (!file) { std::cerr << "metaas: cannot write " << output << '\n'; return 3; }
            sink=&file;
        }
        if (format=="bin") {
            if (!bytes.empty()) sink->write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        } else sink->write(text.data(),static_cast<std::streamsize>(text.size()));
        sink->flush();
        if (!*sink) { std::cerr << "metaas: output write failed\n"; return 3; }
        if (file.is_open()) { file.close(); if (!file) { std::cerr << "metaas: output close failed\n"; return 3; } }
        return 0;
    } catch (const std::length_error& e) { std::cerr << "metaas: " << e.what() << '\n'; return 1; }
    catch (const std::exception& e) { std::cerr << "metaas: " << e.what() << '\n'; return 3; }
}
