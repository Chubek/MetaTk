#include "Driver.hpp"
#include <fstream>
#include <sstream>
#include <cctype>

namespace gppx::driver {
namespace {
std::string trim(const std::string& s) {
    auto first=s.find_first_not_of(" \t\r\n");
    if (first==s.npos) return {};
    return s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);
}
std::string scalar(const std::string& s,size_t line) {
    auto text=trim(s);
    if (text.empty()) return {};
    if (text[0]!='\'' && text[0]!='"') return text;
    const char quote=text[0];
    if (text.size()<2 || text.back()!=quote)
        throw Error("config line "+std::to_string(line)+": unterminated string");
    std::string result;
    for (size_t i=1;i+1<text.size();++i) {
        if (text[i]=='\\' && quote=='"' && i+2<text.size()) {
            char c=text[++i];
            switch (c) {case 'n':result+='\n';break;case 't':result+='\t';break;
                       case 'r':result+='\r';break;default:result+=c;break;}
        } else result+=text[i];
    }
    return result;
}
std::string stripComment(std::string s) {
    char quote=0;bool escape=false;
    for (size_t i=0;i<s.size();++i) {
        char ch=s[i];
        if (quote) {
            if (escape) escape=false;
            else if (ch=='\\' && quote=='"') escape=true;
            else if (ch==quote) quote=0;
        } else if (ch=='\''||ch=='"') quote=ch;
        else if (ch=='#' && (i==0 || std::isspace(static_cast<unsigned char>(s[i-1])))) {
            s.resize(i);break;
        }
    }
    return s;
}
}
/** Parse the documented dependency-free YAML subset:
 *  - flat engine option: `max_depth: 64`
 *  - `syntax:`, `options:`, `defines:` mappings
 *  - `plugins:`, `include_paths:` sequences (relative to config file)
 *  - `plugin:` and `include_path:` single-file shorthand
 * Unlike full YAML, nested maps/sequences, anchors and folded strings are
 * intentionally rejected rather than interpreted incorrectly.
 */
void config(Engine& engine,const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw Error("cannot open config: "+path.string());
    const auto base=std::filesystem::absolute(path).parent_path();
    std::string raw,section;size_t line=0;
    while (std::getline(in,raw)) {
        ++line;
        auto first=raw.find_first_not_of(' ');
        if (first==std::string::npos) continue;
        if (raw[first]=='\t') throw Error("config line "+std::to_string(line)+": indentation must use spaces");
        std::string text=trim(stripComment(raw));
        if (text.empty()) continue;
        const size_t indent=first;
        if (indent==0) section.clear();
        else if (indent!=2 || section.empty())
            throw Error("config line "+std::to_string(line)+": expected a 2-space section entry");
        if (text[0]=='-') {
            if (indent!=2 || (section!="plugins" && section!="include_paths"))
                throw Error("config line "+std::to_string(line)+": list item outside plugins/include_paths");
            if (text.size()>1 && text[1]!=' ')
                throw Error("config line "+std::to_string(line)+": expected '- value'");
            auto item=scalar(text.substr(1),line);
            if (item.empty()) throw Error("config line "+std::to_string(line)+": empty path");
            if (section=="plugins") plugins(engine,{base/item});
            else engine.includePath(base/item);
            continue;
        }
        auto sep=text.find(':');
        if (sep==std::string::npos) throw Error("config line "+std::to_string(line)+": missing ':'");
        auto key=trim(text.substr(0,sep));
        auto value=scalar(text.substr(sep+1),line);
        if (key.empty()) throw Error("config line "+std::to_string(line)+": empty key");
        if (indent==0 && value.empty()) {
            if (key=="syntax"||key=="options"||key=="defines"||key=="plugins"||key=="include_paths") {
                section=key;continue;
            }
            throw Error("config line "+std::to_string(line)+": unrecognized or empty section: "+key);
        }
        if (indent==2) {
            if (section=="syntax" || section=="options") engine.set(key,value);
            else if (section=="defines") engine.define(key,value);
            else throw Error("config line "+std::to_string(line)+": expected list item in "+section);
        } else if (key=="include_path") engine.includePath(base/value);
        else if (key=="plugin") plugins(engine,{base/value});
        else if (key.rfind("syntax.",0)==0) engine.set(key.substr(7),value);
        else engine.set(key,value);
    }
    if (in.bad()) throw Error("I/O error while reading config: "+path.string());
}
}
