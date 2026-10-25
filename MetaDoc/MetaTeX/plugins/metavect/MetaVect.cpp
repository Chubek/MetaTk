#include "MetaTeX.h"
#include <cmath>
#include <locale>
#include <sstream>
#include <stdexcept>

using namespace metatex;
namespace {
double widthOption(const std::string& option){
    if(option.empty())return 0;
    if(option.rfind("width=",0)!=0)throw std::runtime_error("expected [width=POINTS]");
    std::istringstream in(option.substr(6));in.imbue(std::locale::classic());
    double width=0;in>>std::noskipws>>width;
    if(!in||!in.eof()||!std::isfinite(width)||width<0.01||width>1000000)
        throw std::runtime_error("width must be a finite number of points in [0.01, 1000000]");
    return width;
}
void graphic(Engine& engine,bool file){
    auto option=engine.readOptionalText();auto source=engine.readTextArgument();auto caption=engine.readTextArgument();
    try {
        double width=widthOption(option);
        if(file){
            std::filesystem::path path=source;
            if(path.extension()==".mvct")throw std::runtime_error(".mvct evaluation is not implemented; supply a .mvs scene artifact");
            if(path.is_relative()&&engine.currentFile()!="<memory>")path=engine.currentFile().parent_path()/path;
            source=readFile(path,4*1024*1024);
        }
        engine.addGraphic(metavect::Scene(source),std::move(caption),width);
    }catch(const std::exception& ex){engine.report(std::string("MetaVect: ")+ex.what(),true);}
}
}
extern "C" const char* metatex_plugin_id(){return "metavect";}
extern "C" void metatex_register_plugin(Engine* engine){
    engine->registerCommand("metavect",[](Engine& e){graphic(e,false);});
    engine->registerCommand("metavectfile",[](Engine& e){graphic(e,true);});
}
