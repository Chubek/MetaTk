#pragma once
#include "Lexing.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <set>
namespace archtool {
namespace fs=std::filesystem;
inline std::string read_text(const fs::path &p){std::ifstream in(p,std::ios::binary);if(!in)throw lex::Error("cannot read "+p.string());std::ostringstream os;os<<in.rdbuf();if(!in.good()&&!in.eof())throw lex::Error("read error: "+p.string());return os.str();}
inline fs::path anchored(const fs::path &base,const std::string &value){fs::path p(value);return p.is_absolute()?p:base/p;}
inline std::vector<std::string> words(const std::string &source){auto v=lex::Scanner({{"whitespace","[ \t\r\n]+",true},{"word","[^ \t\r\n]+",false}}).scan(source,true,true);std::vector<std::string> result;for(auto&t:v)result.push_back(t.text);return result;}
// Response file expansion: shell-free argument splitting, recursive with cycle check.
inline void expand_args(const std::vector<std::string>&args,std::vector<std::string>&out,std::set<fs::path>&active,fs::path base=fs::current_path(),int depth=0){
  if(depth>24)throw lex::Error("response file nesting exceeds 24");
  for(const auto &a:args){if(a.empty()||a[0]!='@'){out.push_back(a);continue;}
    fs::path p=fs::absolute(anchored(base,a.substr(1))).lexically_normal();if(!active.insert(p).second)throw lex::Error("recursive response file: "+p.string());
    auto sub=words(read_text(p));expand_args(sub,out,active,p.parent_path(),depth+1);active.erase(p);
  }
}
struct ScriptAction {
  enum class Kind { Input, SearchDir, Library };
  Kind kind;
  std::string value;
};
// Preserve directive order across INCLUDE boundaries. Splitting libraries,
// search directories and inputs into separate lists changes lookup semantics.
struct ScriptValues{std::vector<ScriptAction> actions;std::string entry, output, target, base;};
inline void parse_script(const fs::path& file,ScriptValues &res,std::set<fs::path>&active,int depth=0){
  if(depth>24)throw lex::Error("link script nesting exceeds 24");
  auto p=fs::absolute(file).lexically_normal();if(!active.insert(p).second)throw lex::Error("recursive link script: "+p.string());
  auto toks=lex::script_lexer().scan(read_text(p),true,true);
  std::size_t i=0;auto fail=[&](const std::string&msg)->void {throw lex::Error(p.string()+": "+msg+" near token "+std::to_string(i));};
  while(i<toks.size()) {
    if(toks[i].kind!="word")fail("expected directive name"); auto name=toks[i++].text;
    if(i==toks.size()||toks[i++].kind!="lparen")fail("expected '('");
    std::vector<std::string> values;
    while(i<toks.size() && toks[i].kind!="rparen") {
      if(toks[i].kind!="word"&&toks[i].kind!="string")fail("expected string or path");
      values.push_back(toks[i++].text);
      if(i<toks.size()&&toks[i].kind=="comma")++i;
    }
    if(i==toks.size()||toks[i++].kind!="rparen")fail("expected ')'");
    auto single=[&]()->const std::string& {if(values.size()!=1)fail(name+" requires exactly one argument");return values.front();};
    if(name=="INPUT"||name=="GROUP")for(auto &s:values)res.actions.push_back({ScriptAction::Kind::Input,anchored(p.parent_path(),s).string()});
    else if(name=="SEARCH_DIR")res.actions.push_back({ScriptAction::Kind::SearchDir,anchored(p.parent_path(),single()).string()});
    else if(name=="LIB")for(auto &s:values)res.actions.push_back({ScriptAction::Kind::Library,s});
    else if(name=="ENTRY")res.entry=single();
    else if(name=="OUTPUT")res.output=anchored(p.parent_path(),single()).string();
    else if(name=="TARGET")res.target=single();
    else if(name=="BASE")res.base=single();
    else if(name=="INCLUDE")parse_script(anchored(p.parent_path(),single()),res,active,depth+1);
    else fail("unknown linker directive "+name);
  }
  active.erase(p);
}
}
