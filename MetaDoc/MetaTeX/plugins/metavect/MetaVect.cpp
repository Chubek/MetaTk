#include "MetaTeX.h"
#include <algorithm>
#include <cmath>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace metatex;
namespace {
struct Options {double width=0,height=0,scale=0;};
std::string trim(std::string text){
    auto first=text.find_first_not_of(" \t\r\n");
    return first==std::string::npos?"":text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
}
double measurement(std::string text,bool dimension){
    text=trim(std::move(text));std::istringstream in(text);in.imbue(std::locale::classic());
    double value=0;in>>value;
    if(!in||!std::isfinite(value))throw std::runtime_error("expected a finite positive measurement");
    std::string unit;std::getline(in,unit);unit=trim(std::move(unit));
    double factor=1;
    if(dimension){
        if(unit.empty()||unit=="bp")factor=1;
        else if(unit=="pt")factor=72/72.27;
        else if(unit=="in")factor=72;
        else if(unit=="cm")factor=72/2.54;
        else if(unit=="mm")factor=72/25.4;
        else throw std::runtime_error("unknown graphic unit: "+unit);
    }else if(!unit.empty())throw std::runtime_error("scale must be unitless");
    value*=factor;
    if(!std::isfinite(value)||value<=0||value>1000000||(dimension&&value<0.01))
        throw std::runtime_error("graphic measurement is outside the supported range");
    return value;
}
Options options(const std::string& text){
    Options result;std::set<std::string> seen;
    if(trim(text).empty())return result;
    std::size_t at=0;
    while(at<=text.size()){
        auto end=text.find(',',at);if(end==std::string::npos)end=text.size();
        auto entry=text.substr(at,end-at);auto equals=entry.find('=');
        if(equals==std::string::npos)throw std::runtime_error("expected key=value graphic option");
        auto key=trim(entry.substr(0,equals));
        if(!seen.insert(key).second)throw std::runtime_error("duplicate graphic option: "+key);
        if(key!="width"&&key!="height"&&key!="scale")throw std::runtime_error("unknown graphic option: "+key);
        auto value=measurement(entry.substr(equals+1),key!="scale");
        if(key=="width")result.width=value;else if(key=="height")result.height=value;else result.scale=value;
        if(end==text.size())break;
        at=end+1;
    }
    if(result.scale&&(result.width||result.height))throw std::runtime_error("scale cannot be combined with width or height");
    return result;
}
double displayWidth(const metavect::Scene& scene,const Options& opts){
    auto view=scene.view();double width=opts.width;
    if(opts.height){auto fitted=opts.height*view.width/view.height;width=width?std::min(width,fitted):fitted;}
    if(opts.scale)width=view.width*opts.scale;
    if(!std::isfinite(width)||width<0||(width>0&&width<0.01)||width>1000000)
        throw std::runtime_error("computed graphic width must be 0.01..1000000 points");
    return width;
}
metavect::Scene scene(Engine& e,std::string source,bool file){
    if(file){
        std::filesystem::path path=source;
        if(path.extension()==".mvct")throw std::runtime_error(".mvct evaluation is not implemented; supply a .mvs scene artifact");
        if(path.is_relative()&&e.currentFile()!="<memory>")path=e.currentFile().parent_path()/path;
        source=readFile(path,4*1024*1024);
    }
    if(source.size()>4*1024*1024)throw std::runtime_error("scene size limit exceeded");
    return metavect::Scene(source);
}
template<class F> void checked(Engine& e,F action){
    try{action();}catch(const std::exception& ex){e.report(std::string("MetaVect: ")+ex.what(),true);}
}
void graphic(Engine& e,bool file){
    auto option=e.readOptionalText();auto source=e.readTextArgument();auto caption=e.readTextArgument();
    checked(e,[&]{auto opts=options(option);auto value=scene(e,std::move(source),file);
        auto width=displayWidth(value,opts);e.addGraphic(std::move(value),std::move(caption),width);});
}
void define(Engine& e,bool file){
    auto option=e.readOptionalText();auto name=e.readArgument();auto source=e.readTextArgument();
    checked(e,[&]{
        if(name.size()!=1||name[0].kind!=TokenKind::Control||name[0].active)throw std::runtime_error("scene definition expects one control sequence");
        auto command=name[0].controlName();if(e.hasCommand(command))throw std::runtime_error("command already defined: "+command);
        auto defaults=options(option);auto value=scene(e,std::move(source),file);displayWidth(value,defaults);
        e.defineCommand(command,[value,defaults](Engine& x){
            auto option=x.readOptionalArgument();auto caption=x.readTextArgument();
            checked(x,[&]{auto opts=option?options(x.tokensText(*option)):defaults;
                x.addGraphic(value,std::move(caption),displayWidth(value,opts));});
        });
    });
}
}
extern "C" const char* metatex_plugin_id(){return "metavect";}
extern "C" void metatex_register_plugin(Engine* engine){
    engine->registerCommand("metavect",[](Engine& e){graphic(e,false);});
    engine->registerCommand("metavectfile",[](Engine& e){graphic(e,true);});
    engine->registerCommand("metavectdef",[](Engine& e){define(e,false);});
    engine->registerCommand("metavectdeffile",[](Engine& e){define(e,true);});
    engine->registerEnvironment("metavect",[](Engine& e){
        auto option=e.readOptionalText();auto caption=e.readTextArgument();auto source=e.readLiteralEnvironment("metavect");
        checked(e,[&]{auto opts=options(option);auto value=scene(e,std::move(source),false);
            auto width=displayWidth(value,opts);e.addGraphic(std::move(value),std::move(caption),width);});
        e.endEnvironment("metavect");
    });
}
