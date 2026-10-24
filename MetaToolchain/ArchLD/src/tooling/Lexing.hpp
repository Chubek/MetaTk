#pragma once
// Shared DSLtk-backed scanner for ArchToolchain drivers and scripting.
// The DSLtk PEG subset is greedy rather than a POSIX/PCRE regular expression.
#include "DSLtk.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace archtool::lex {
struct Token {std::string kind, text; std::size_t offset{}, line{}, column{};};
struct Rule {std::string kind, pattern; bool skip=false;};
class Error : public std::runtime_error {public: using std::runtime_error::runtime_error;};
class Scanner {
  struct Compiled {std::string kind; dsl::peg_detail::CompiledPattern p; bool skip;};
  std::vector<Compiled> rules_;
public:
  explicit Scanner(const std::vector<Rule>& rules) {
    for (const auto &r: rules) {
      auto p=dsl::peg_detail::CompiledPattern::compile(r.pattern);
      if (!p.valid || r.pattern.empty() || p.match_at("",0).has_value())
        throw Error("invalid or zero-length lexer rule '"+r.kind+"': "+p.error);
      rules_.push_back({r.kind,std::move(p),r.skip});
    }
  }
  std::vector<Token> scan(std::string_view source, bool allow_strings=false, bool allow_comments=false) const {
    std::vector<Token> out;
    std::size_t pos=0,line=1,column=1;
    auto advance=[&](std::string_view s) {for(char c:s) {if(c=='\n'){++line;column=1;}else ++column;}};
    while(pos<source.size()) {
      if(allow_comments && source[pos]=='#') {auto end=source.find('\n',pos); if(end==std::string_view::npos) end=source.size();advance(source.substr(pos,end-pos));pos=end;continue;}
      if(allow_strings && (source[pos]=='\'' || source[pos]=='"')) {
        const char quote=source[pos];std::size_t start=pos,l=line,c=column;std::string content;
        ++pos;advance(source.substr(start,1)); bool done=false;
        while(pos<source.size()) {
          char b=source[pos];++pos;advance(source.substr(pos-1,1));
          if(b==quote) {done=true;break;}
          if(b=='\\') {if(pos==source.size()) break; char escaped=source[pos];++pos;advance(source.substr(pos-1,1));
            switch(escaped){case 'n':content+='\n';break;case 'r':content+='\r';break;case 't':content+='\t';break;default:content+=escaped;break;}
          } else content+=b;
        }
        if(!done) throw Error("unterminated quoted string at "+std::to_string(l)+":"+std::to_string(c));
        out.push_back({"string",std::move(content),start,l,c});continue;
      }
      std::size_t best=pos;const Compiled *picked=nullptr;
      for(const auto &r:rules_) {auto end=r.p.match_at(source,pos);if(end && *end>best){best=*end;picked=&r;}}
      if(!picked) throw Error("unexpected byte at "+std::to_string(line)+":"+std::to_string(column));
      auto text=source.substr(pos,best-pos);
      if(!picked->skip)out.push_back({picked->kind,std::string(text),pos,line,column});
      advance(text);pos=best;
    }
    return out;
  }
};
inline Scanner script_lexer(){return Scanner({{"whitespace","[ \t\r\n]+",true},{"word","[a-zA-Z0-9_./:+-]+",false},{"lparen","(",false},{"rparen",")",false},{"comma",",",false}});}
}
