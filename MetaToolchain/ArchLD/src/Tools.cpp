// Shared ArchToolchain command-line frontends. Linker remains the CWEB C engine.
#include "ArchLD.h"
#include "ArchBFD.h"
#include "tooling/Script.hpp"
#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>
namespace fs=std::filesystem;
using namespace archtool;
namespace {
std::string name_of(const char*p){return fs::path(p).filename().string();}
std::string catalogue_path(const char *argv0){
  char buf[8192];auto count=readlink("/proc/self/exe",buf,sizeof(buf)-1);
  if(count>0){buf[count]=0;return (fs::path(buf).parent_path()/"../targets/ArchBFDCatalogue.json").lexically_normal().string();}
  auto p=fs::path(argv0);if(p.has_parent_path())return (fs::absolute(p).parent_path()/"../targets/ArchBFDCatalogue.json").lexically_normal().string();
  return "targets/ArchBFDCatalogue.json";
}
uint64_t parse_uint(const std::string&s){if(s.empty()||s[0]=='-')throw lex::Error("invalid unsigned address: "+s);errno=0;char*end=nullptr;auto n=strtoull(s.c_str(),&end,0);if(errno||!end||*end)throw lex::Error("invalid unsigned address: "+s);return n;}
void banner(){std::cout<<"ArchToolchain (ArchLD/ArchBFD + DSLtk); ELF64 x86-64 and ELF32 i386 static linking\n";}
struct Deleter {void operator()(ArchLD*p)const{archld_free(p);}};
struct CatalogDeleter{void operator()(ArchBFD*p)const{archbfd_close(p);}};
struct ObjDeleter{void operator()(ArchBFDObject*p)const{archbfd_object_close(p);}};
struct Diags {DsymDiags d;Diags(){dsym_diags_init(&d);}~Diags(){dsym_diags_free(&d);}std::string message()const{std::string s;for(size_t i=0;i<d.count;i++){auto &x=d.items[i];if(x.msg){if(!s.empty())s+='\n';s+=x.msg;}}return s.empty()?"ArchBFD parser rejected the document or input":s;}};
void link_help(){std::cout<<R"(Usage: ArchLD [-o OUT] [-e SYMBOL] [--target ELF64|ELF32] [--base ADDRESS]
              [-L DIR] [-l NAME] [-Map=FILE] [--catalogue FILE]
              [-T SCRIPT] [@response-file] INPUT.o [ARCHIVE.a ...]
Supported script directives: INPUT, GROUP, SEARCH_DIR, LIB, ENTRY, OUTPUT,
TARGET, BASE, INCLUDE.  This is a *restricted* link manifest grammar, not a
GNU ld command language. Only the static ELF32/i386 and ELF64/x86-64 targets
have writers. Unrecognized flags and unimplemented relocations are errors.
)";}
std::string findlib(const std::string &name,const std::vector<std::string>&paths){
 for(const auto &dir:paths){auto p=fs::path(dir)/("lib"+name+".a");std::error_code ec;if(fs::is_regular_file(p,ec))return p.string();}
 throw lex::Error("cannot find -l"+name+"; supply -L or SEARCH_DIR");
}
int run_ld(const std::vector<std::string>&cmd,const std::string&default_catalogue){
  std::string output="a.out",entry="_start",target="ELF64",cat=default_catalogue,map;
  std::string base_text;std::vector<std::string> paths{"."},inputs;
  std::set<fs::path> active;bool endopts=false;
  // Script options are handled in encounter order. Scripts cannot silently
  // override subsequent CLI switches. Library search respects input ordering.
  for(size_t i=0;i<cmd.size();i++){
    const std::string &a=cmd[i];
    auto next=[&]()->std::string{if(i+1>=cmd.size())throw lex::Error("missing argument for "+a);return cmd[++i];};
    if(!endopts&&a=="--"){endopts=true;continue;}
    if(!endopts&&(a=="--help"||a=="-h")){link_help();return 0;}
    if(!endopts&&a=="--version"){banner();return 0;}
    if(!endopts&&(a=="--static"||a=="-static"||a=="--start-group"||a=="--end-group"))continue;
    if(!endopts&&(a=="-o"||a=="--output")){output=next();continue;}
    if(!endopts&&(a=="-e"||a=="--entry")){entry=next();continue;}
    if(!endopts&&a=="--catalogue"){cat=next();continue;}
    if(!endopts&&a=="--target"){target=next();continue;}
    if(!endopts&&a=="--base"){base_text=next();continue;}
    if(!endopts&&a=="--map"){map=next();continue;}
    if(!endopts&&a.rfind("-Map=",0)==0){map=a.substr(5);continue;}
    if(!endopts&&(a=="-T"||a=="--script")){ScriptValues tmp;parse_script(next(),tmp,active);
      for(const auto &action:tmp.actions){
        switch(action.kind){
          case ScriptAction::Kind::Input: inputs.push_back(action.value);break;
          case ScriptAction::Kind::SearchDir: paths.push_back(action.value);break;
          case ScriptAction::Kind::Library: inputs.push_back(findlib(action.value,paths));break;
        }
      }
      if(!tmp.output.empty())output=tmp.output;if(!tmp.entry.empty())entry=tmp.entry;if(!tmp.target.empty())target=tmp.target;if(!tmp.base.empty())base_text=tmp.base;continue;}
    if(!endopts&&a=="-L"){paths.push_back(next());continue;}
    if(!endopts&&a.rfind("-L",0)==0&&a.size()>2){paths.push_back(a.substr(2));continue;}
    if(!endopts&&a=="-l"){inputs.push_back(findlib(next(),paths));continue;}
    if(!endopts&&a.rfind("-l",0)==0&&a.size()>2){inputs.push_back(findlib(a.substr(2),paths));continue;}
    if(!endopts&&!a.empty()&&a[0]=='-')throw lex::Error("unknown linker option: "+a);
    inputs.push_back(a);
  }
  if(inputs.empty())throw lex::Error("no input files (see ArchLD --help)");
  const auto base=base_text.empty()? (target=="ELF32"?0x08049000ull:0x401000ull):parse_uint(base_text);
  std::unique_ptr<ArchLD,Deleter> ld(archld_new(cat.c_str(),target.c_str(),base));
  if(!ld)throw lex::Error("linker allocation failed");
  auto check=[&](bool ok){if(!ok)throw lex::Error(archld_error(ld.get())[0]?archld_error(ld.get()):"linker error");};
  check(!archld_error(ld.get())[0]);
  for(const auto &f:inputs)check(archld_add(ld.get(),f.c_str()));
  check(archld_link(ld.get()));
  uint64_t at=0;if(!archld_symbol(ld.get(),entry.c_str(),&at))throw lex::Error("entry point '"+entry+"' undefined");
  check(archld_write_elf(ld.get(),output.c_str(),at));
  if(!map.empty())check(archld_write_map_file(ld.get(),map.c_str()));
  std::cout<<"ArchLD: linked "<<output<<" (entry "<<entry<<" = 0x"<<std::hex<<at<<std::dec<<", "<<archld_image_size(ld.get())<<" bytes in memory)\n";
  return 0;
}
struct ObjectOptions {std::string cat,target="ELF64";bool verbose=false;std::vector<std::string> files;};
ObjectOptions object_args(const std::vector<std::string>&cmd,const std::string&def){
 ObjectOptions o;o.cat=def;for(size_t i=0;i<cmd.size();i++){auto a=cmd[i];auto next=[&](){if(i+1==cmd.size())throw lex::Error("missing argument for "+a);return cmd[++i];};
  if(a=="--catalogue")o.cat=next();else if(a=="--target")o.target=next();else if(a=="-v"||a=="--verbose")o.verbose=true;
  else if(a=="--help")throw lex::Error("usage: ArchNm|ArchSize|ArchRelocs [--target ELF64|ELF32] FILE.o ...");
  else if(!a.empty()&&a[0]=='-')throw lex::Error("unknown option "+a);else o.files.push_back(a);
 }if(o.files.empty())throw lex::Error("missing object filename");return o;
}
int run_object(const std::string &mode,const std::vector<std::string>&cmd,const std::string&def){
 const auto o=object_args(cmd,def);Diags diag;std::unique_ptr<ArchBFD,CatalogDeleter> catalog(archbfd_open(o.cat.c_str(),&diag.d));if(!catalog)throw lex::Error(diag.message());
 auto spec=archbfd_load_name(catalog.get(),o.target.c_str(),0,&diag.d);if(!spec)throw lex::Error(diag.message());
 for(const auto &p:o.files){
  std::unique_ptr<ArchBFDObject,ObjDeleter> object(archbfd_object_open(p.c_str(),spec,&diag.d));
  if(!object)throw lex::Error(p+": "+diag.message());
  if(o.files.size()>1)std::cout<<p<<":\n";
  if(mode=="ArchNm"){
   for(size_t i=0;i<archbfd_object_symbol_count(object.get());i++){
    const auto *s=archbfd_object_symbol(object.get(),i);
    if(!s||!s->name||!*s->name)continue;
    char t='?';if(s->shndx==0)t='U';else if(s->shndx==-0x0fff||s->shndx==0xfff1)t='A';else if(s->shndx>=0){
     const auto *sec=archbfd_object_section(object.get(),size_t(s->shndx));
     if(sec){t=(sec->flags&4)?'T':(sec->type==8?'B':(sec->flags&1?'D':'R'));}
    }
    if(s->bind&&std::strcmp(s->bind,"weak")==0)t='W';
    else if(s->bind&&std::strcmp(s->bind,"local")==0)t=char(std::tolower((unsigned char)t));
    std::cout<<std::hex<<std::setw(16)<<std::setfill('0')<<uint64_t(s->value)<<std::dec<<" "<<t<<" "<<s->name<<"\n";
   }
  }else if(mode=="ArchSize"){
    uint64_t text=0,data=0,bss=0;
    for(size_t i=0;i<archbfd_object_section_count(object.get());i++){
      auto *sec=archbfd_object_section(object.get(),i);if(!sec||(sec->flags&2)==0||sec->size<0)continue;
      if(sec->type==8)bss+=uint64_t(sec->size);else if(sec->flags&1)data+=uint64_t(sec->size);else text+=uint64_t(sec->size);
    }
    std::cout<<"text="<<text<<" data="<<data<<" bss="<<bss<<" total="<<(text+data+bss)<<" "<<p<<"\n";
  }else if(mode=="ArchSections"){
    std::cout<<"index\tname\ttype\tflags\tsize\talign\n";
    for(size_t i=0;i<archbfd_object_section_count(object.get());i++){
      auto *sec=archbfd_object_section(object.get(),i);if(!sec)continue;
      std::cout<<i<<"\t"<<(sec->name?sec->name:"")<<"\t"<<sec->type<<"\t0x"<<std::hex<<sec->flags<<std::dec<<"\t"<<sec->size<<"\t"<<sec->align<<"\n";
    }
  }else if(mode=="ArchRelocs"){
    for(size_t i=0;i<archbfd_object_reloc_count(object.get());i++){
      auto *r=archbfd_object_reloc(object.get(),i);
      std::cout<<"0x"<<std::hex<<uint64_t(r->offset)<<std::dec<<" "<<(r->type_name?r->type_name:"UNKNOWN")<<" "<<(r->symbol_name?r->symbol_name:"<unnamed>")<<" addend="<<r->addend<<"\n";
    }
  }
 }
 return 0;
}
int run_targets(const std::vector<std::string>&cmd,const std::string&def){
 std::string cat=def;bool verbose=false;
 for(size_t i=0;i<cmd.size();i++){if(cmd[i]=="--catalogue"){if(i+1==cmd.size())throw lex::Error("missing catalogue");cat=cmd[++i];}
 else if(cmd[i]=="--verbose")verbose=true;else throw lex::Error("usage: ArchTargets [--catalogue FILE] [--verbose]");}
 Diags ds;std::unique_ptr<ArchBFD,CatalogDeleter> db(archbfd_open(cat.c_str(),&ds.d));if(!db)throw lex::Error(ds.message());
 for(size_t i=0;i<archbfd_count(db.get());i++){
   auto*e=archbfd_entry(db.get(),i);
   std::cout<<(e->name?e->name:"?")<<"\t"<<(e->format?e->format:"?")<<"\t"<<(e->arch?e->arch:"?");
   if(verbose){auto*s=archbfd_load(db.get(),e,0,&ds.d);if(!s)throw lex::Error(ds.message());std::cout<<"\t"<<s->reloc_count<<" relocations\t"<<s->record_count<<" records";}
   std::cout<<"\n";
 }
 return 0;
}
int run_spec(const std::vector<std::string>&cmd,const std::string&def){
 std::string cat=def,target="ELF64";bool show_all=false;
 for(size_t i=0;i<cmd.size();++i){auto a=cmd[i];
   if(a=="--catalogue"||a=="--target"){
     if(i+1==cmd.size())throw lex::Error("missing value for "+a);
     if(a=="--catalogue")cat=cmd[++i];else target=cmd[++i];
   }else if(a=="--all")show_all=true;
   else throw lex::Error("usage: ArchSpec --target FORMAT [--all] [--catalogue FILE]");
 }
 Diags d;std::unique_ptr<ArchBFD,CatalogDeleter> db(archbfd_open(cat.c_str(),&d.d));
 if(!db)throw lex::Error(d.message());
 const auto *spec=archbfd_load_name(db.get(),target.c_str(),0,&d.d);
 if(!spec)throw lex::Error(d.message());
 std::cout<<"name "<<(spec->name?spec->name:"<unnamed>")<<"\n"
          <<"arch "<<(spec->arch?spec->arch:"<unknown>")<<"\n"
          <<"format "<<archbfd_format_name(spec->format)<<"\n"
          <<"byte_order "<<archbfd_byte_order_name(spec->byte_order)<<"\n"
          <<"vector "<<(spec->target_vector?spec->target_vector:"<unknown>")<<"\n"
          <<"relocations "<<spec->reloc_count<<"\n";
 for(size_t i=0;i<spec->reloc_count;i++){
   auto &r=spec->relocations[i];
   std::cout<<"  "<<r.number<<" "<<(r.name?r.name:"<unnamed>")<<" bytes="<<r.size
      <<" bits="<<r.bitsize<<" pcrel="<<r.pcrel<<" overflow="<<archbfd_overflow_name(r.overflow)<<"\n";
 }
 if(show_all){
   for(size_t i=0;i<spec->record_count;i++){
     auto &r=spec->records[i];std::cout<<"record "<<(r.name?r.name:"")<<" size="<<r.size<<"\n";
     for(size_t j=0;j<r.field_count;j++){
       auto &f=r.fields[j];std::cout<<"  field "<<(f.name?f.name:"")<<" at="<<f.offset<<" size="<<f.size<<"\n";
     }
   }
 }
 return 0;
}
int run_lex(const std::vector<std::string>&args){
 std::vector<lex::Rule> rules;std::string path;bool comments=false;
 for(size_t i=0;i<args.size();i++){
  const auto&a=args[i];if(a=="--rule"){
   if(i+1==args.size())throw lex::Error("--rule needs NAME=PATTERN");auto val=args[++i];auto eq=val.find('=');if(eq==std::string::npos||!eq)throw lex::Error("expected NAME=PATTERN");
   rules.push_back({val.substr(0,eq),val.substr(eq+1),val[0]=='_'});
  }else if(a=="--comments")comments=true;else if(a=="--help"){std::cout<<"Usage: ArchLex [--rule NAME=DSLtk-PEG-pattern ...] [--comments] FILE\n";return 0;}
  else if(a[0]=='-')throw lex::Error("unknown lex option: "+a);else path=a;
 }
 if(path.empty())throw lex::Error("usage: ArchLex [--rule NAME=PATTERN] FILE");
 if(rules.empty())rules={{"_space","[ \t\r\n]+",true},{"ident","[a-zA-Z_][a-zA-Z0-9_]*",false},{"integer","[0-9]+",false},{"punct",".",false}};
 const auto scanner=lex::Scanner(rules);auto source=read_text(path);for(const auto&t:scanner.scan(source,true,comments))
 std::cout<<t.line<<':'<<t.column<<"\t"<<t.kind<<"\t"<<std::quoted(t.text)<<'\n';
 return 0;
}
}
int main(int argc,char**argv){
 try{
  auto mode=name_of(argv[0]);std::vector<std::string> raw;
  for(int i=1;i<argc;i++)raw.emplace_back(argv[i]);
  if(mode=="ArchTool"){
    if(raw.empty()){banner();std::cerr<<"usage: ArchTool {ld|nm|size|sections|relocs|targets|spec|lex} ...\n";return 2;}
    auto chosen=raw.front();raw.erase(raw.begin());
    if(chosen=="ld")mode="ArchLD";else if(chosen=="nm")mode="ArchNm";else if(chosen=="size")mode="ArchSize";
    else if(chosen=="relocs")mode="ArchRelocs";else if(chosen=="sections")mode="ArchSections";else if(chosen=="targets")mode="ArchTargets";else if(chosen=="spec")mode="ArchSpec";else if(chosen=="lex")mode="ArchLex";
    else throw lex::Error("unknown subcommand: "+chosen);
  }
  std::vector<std::string> args;std::set<fs::path> visited;expand_args(raw,args,visited);
  auto cat=catalogue_path(argv[0]);
  if(mode=="ArchLD")return run_ld(args,cat);
  if(mode=="ArchTargets")return run_targets(args,cat);
  if(mode=="ArchSpec")return run_spec(args,cat);
  if(mode=="ArchLex")return run_lex(args);
  if(mode=="ArchNm"||mode=="ArchSize"||mode=="ArchRelocs"||mode=="ArchSections")return run_object(mode,args,cat);
  throw lex::Error("unknown tool name: "+mode);
 }catch(const std::exception&e){std::cerr<<"ArchToolchain: "<<e.what()<<'\n';return 1;}
}
