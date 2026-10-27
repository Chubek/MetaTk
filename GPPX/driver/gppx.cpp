/** GPPX command-line interface: argument ordering is significant.
 * --config may set defaults; later -D, -I, --profile etc override them.
 */
#include "Driver.hpp"
#include "GPPX-Macro.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
void help() {
    std::cout << R"(GPPX - extensible, syntax-retargetable preprocessor
Usage: gppx [options] [input-file]
  -o, --output FILE       Write bytes to FILE (- for stdout; default: stdout)
  -e, --eval TEXT         Process TEXT instead of a file
  -DNAME[=VALUE]         Define an object macro (default VALUE=1)
  -UNAME                 Undefine an object macro
  -IDIR                  Add an include directory
  -P, --plugin FILE      Load a native shared-library plugin
  --plugin-dir DIR        Load .so/.dylib/.dll plugins, sorted by filename
  -c, --config FILE      Load configuration (documented YAML subset)
   --profile NAME         Syntax profile: c, percent, template, jackscript
  --prefix TEXT          Override directive prefix
  --macros STYLE         bare, dollar, mustache
  --max-depth NUMBER     Macro expansion limit
  --max-includes NUMBER  Include nesting limit
  --max-output BYTES     Output size limit
  --no-strict            Preserve unknown directives instead of an error
  --check                Validate/process without writing output
  --repl                 Run interactive preprocessor
  --version              Print version and plugin ABI
  --help                 Print this help
  --                     Stop option parsing (permits input starting with '-')
If no input or --eval is given, reads stdin. Options apply in their listed
order. Plugins contain native code; do not load files from untrusted sources.
)";
}
void writeFile(const std::string& path,const std::string& data) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if (!out) throw gppx::Error("cannot open output file: "+path);
    out.write(data.data(),static_cast<std::streamsize>(data.size()));
    out.flush();
    if (!out) throw gppx::Error("failed writing output file: "+path);
}
}
int main(int argc,char** argv) {
    try {
        gppx::Engine engine;
        std::string input,output,expression;
        bool eval=false,repl=false,check=false,options=true;
        for (int i=1;i<argc;++i) {
            std::string arg=argv[i];
            auto next=[&]()->std::string {
                if (i+1>=argc) throw gppx::Error("missing argument after "+arg);
                return argv[++i];
            };
            if (options && arg=="--") {options=false;continue;}
            if (options && (arg=="--help"||arg=="-h")) {help();return 0;}
            if (options && arg=="--version") {std::cout<<"GPPX 1.0.1 (plugin ABI "<<GPPX_ABI_VERSION<<")\n";return 0;}
            if (options && arg=="--repl") repl=true;
            else if (options && arg=="--check") check=true;
            else if (options && (arg=="-o"||arg=="--output")) output=next();
            else if (options && (arg=="-e"||arg=="--eval")) {expression=next();eval=true;}
            else if (options && (arg=="-c"||arg=="--config")) gppx::driver::config(engine,next());
            else if (options && (arg=="-P"||arg=="--plugin"||arg=="--plugin-dir")) gppx::driver::plugins(engine,{next()});
            else if (options && arg=="--profile") engine.set("profile",next());
            else if (options && arg=="--prefix") engine.set("directive",next());
            else if (options && arg=="--macros") engine.set("macro_style",next());
            else if (options && arg=="--max-output") engine.set("max_output",next());
            else if (options && arg=="--max-depth") engine.set("max_depth",next());
            else if (options && arg=="--max-includes") engine.set("max_includes",next());
            else if (options && arg=="--no-strict") engine.set("strict","false");
            else if (options && (arg=="-D"||arg.rfind("-D",0)==0)) {
                auto spec=arg=="-D"?next():arg.substr(2);
                auto sep=spec.find('=');
                auto name=spec.substr(0,sep);
                if (!gppx_directive_valid_name(name.c_str())) throw gppx::Error("invalid -D identifier: "+name);
                engine.define(name,sep==spec.npos?"1":spec.substr(sep+1));
            }
            else if (options && (arg=="-U"||arg.rfind("-U",0)==0)) engine.undef(arg=="-U"?next():arg.substr(2));
            else if (options && (arg=="-I"||arg.rfind("-I",0)==0)) engine.includePath(arg=="-I"?next():arg.substr(2));
            else if (options && !arg.empty()&&arg[0]=='-' && arg!="-") throw gppx::Error("unknown CLI argument: "+arg);
            else if (!input.empty()) throw gppx::Error("only one input file may be specified");
            else input=arg;
        }
        if (repl) {
            if (eval||!input.empty()||check) throw gppx::Error("--repl cannot be combined with input/--eval/--check");
            return gppx::driver::repl(engine);
        }
        if (eval && !input.empty()) throw gppx::Error("input-file and --eval cannot be combined");
        if (check && !output.empty()) throw gppx::Error("--check and --output cannot be combined");
        std::string result;
        if (eval) result=engine.process(expression,"<eval>");
        else if (!input.empty() && input!="-") result=engine.processFile(input);
        else {
            std::ostringstream data;
            data<<std::cin.rdbuf();
            if (std::cin.bad()) throw gppx::Error("failed reading stdin");
            result=engine.process(data.str(),"<stdin>");
        }
        if (!check) {
            if (output.empty()||output=="-") {
                std::cout.write(result.data(),static_cast<std::streamsize>(result.size()));
                if (!std::cout) throw gppx::Error("failed writing stdout");
            } else writeFile(output,result);
        }
        for (auto& d:engine.diagnostics())
            std::cerr<<d.path<<':'<<d.line<<": "<<(d.warning?"warning: ":"error: ")<<d.message<<'\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<"gppx: "<<e.what()<<'\n';return 1;
    }
}
