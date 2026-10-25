#include "MetaVect-Scene.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Path {
    std::vector<mv_path_command> commands;
    mv_paint stroke{1,0,0,0}, fill{0,0,0,0};
    double width=1;
};
constexpr std::size_t maxCommands=100000, maxPaths=10000, maxBytes=4*1024*1024;
}
struct mv_scene {
    double width=0,height=0;
    std::vector<Path> paths;
    std::vector<mv_path_view> views;
};
namespace {
class Parser {
    std::string source_;
    std::size_t at_=0, line_=1, commands_=0;
    mv_scene scene_;
    mv_paint stroke_{1,0,0,0}, fill_{0,0,0,0};
    double width_=1;
    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error("scene line "+std::to_string(line_)+": "+message);
    }
    void space(){
        while(at_<source_.size()){
            char c=source_[at_];
            if(c=='\n'){++line_;++at_;}
            else if(c==' '||c=='\t'||c=='\r')++at_;
            else break;
        }
    }
    std::string token(){
        space();if(at_==source_.size())fail("unexpected end of scene");
        if(source_[at_]==';'){++at_;return ";";}
        std::size_t start=at_;
        while(at_<source_.size() && source_[at_]!=';' && source_[at_]!=' ' &&
              source_[at_]!='\t' && source_[at_]!='\r' && source_[at_]!='\n')++at_;
        return source_.substr(start,at_-start);
    }
    double numeric(const std::string& text){
        std::istringstream in(text);in.imbue(std::locale::classic());
        double n=0;in>>std::noskipws>>n;
        if(!in || !in.eof() || !std::isfinite(n) || std::abs(n)>1000000)fail("invalid or out-of-range number: "+text);
        return n==0?0:n;
    }
    double number(){return numeric(token());}
    double positive(){double n=number();if(n<0.01)fail("dimension must be at least 0.01 points");return n;}
    void end(){if(token()!=";")fail("expected ';'");}
    mv_paint paint(){
        auto first=token();if(first=="none")return {0,0,0,0};
        mv_paint p{1,numeric(first),number(),number()};
        if(p.red<0||p.red>1||p.green<0||p.green>1||p.blue<0||p.blue>1)fail("color components must be in [0, 1]");
        return p;
    }
    void add(Path& p,mv_path_op op,std::initializer_list<double> values){
        if(++commands_>maxCommands)fail("path command limit exceeded");
        mv_path_command c{op,{}};std::copy(values.begin(),values.end(),c.values);p.commands.push_back(c);
    }
    void path(Path& p){
        bool active=false;
        while(true){
            auto op=token();if(op==";")break;
            if(op=="M"){double x=number(),y=number();add(p,MV_MOVE,{x,y});active=true;}
            else if(!active)fail("path must start with M (also after Z)");
            else if(op=="L"){double x=number(),y=number();add(p,MV_LINE,{x,y});}
            else if(op=="C"){
                double x1=number(),y1=number(),x2=number(),y2=number(),x=number(),y=number();
                add(p,MV_CUBIC,{x1,y1,x2,y2,x,y});
            }else if(op=="Z"){add(p,MV_CLOSE,{});active=false;}
            else fail("unknown path operation: "+op);
        }
        if(p.commands.empty())fail("empty path");
    }
public:
    explicit Parser(std::string source):source_(std::move(source)){}
    mv_scene parse(){
        if(token()!="metavect-scene" || token()!="1")fail("expected metavect-scene version 1 header");
        scene_.width=positive();scene_.height=positive();end();
        while(true){
            space();if(at_==source_.size())break;
            auto op=token();
            if(op=="stroke"){stroke_=paint();end();continue;}
            if(op=="fill"){fill_=paint();end();continue;}
            if(op=="width"){width_=positive();end();continue;}
            if(scene_.paths.size()>=maxPaths)fail("path count limit exceeded");
            Path p;p.stroke=stroke_;p.fill=fill_;p.width=width_;
            if(op=="path")path(p);
            else if(op=="rect"){
                double x=number(),y=number(),w=positive(),h=positive();end();
                add(p,MV_MOVE,{x,y});add(p,MV_LINE,{x+w,y});add(p,MV_LINE,{x+w,y+h});
                add(p,MV_LINE,{x,y+h});add(p,MV_CLOSE,{});
            }else if(op=="circle"){
                double x=number(),y=number(),r=positive();end();double k=r*0.5522847498307936;
                add(p,MV_MOVE,{x+r,y});add(p,MV_CUBIC,{x+r,y+k,x+k,y+r,x,y+r});
                add(p,MV_CUBIC,{x-k,y+r,x-r,y+k,x-r,y});add(p,MV_CUBIC,{x-r,y-k,x-k,y-r,x,y-r});
                add(p,MV_CUBIC,{x+k,y-r,x+r,y-k,x+r,y});add(p,MV_CLOSE,{});
            }else fail("unknown scene command: "+op);
            scene_.paths.push_back(std::move(p));
        }
        for(const auto& p:scene_.paths)scene_.views.push_back({p.commands.data(),p.commands.size(),p.stroke,p.fill,p.width});
        return std::move(scene_);
    }
};
}
extern "C" mv_scene* mv_scene_parse(const char* source,std::size_t length,char* error,std::size_t capacity){
    if(capacity && !error)return nullptr;
    if(capacity)error[0]='\0';
    try {
        if(!source || length>maxBytes)throw std::runtime_error("invalid scene buffer or scene exceeds 4 MiB");
        if(std::memchr(source,'\0',length))throw std::runtime_error("NUL byte in scene");
        return new mv_scene(Parser(std::string(source,length)).parse());
    }catch(const std::exception& ex){
        if(capacity){std::size_t n=std::min(capacity-1,std::strlen(ex.what()));std::memcpy(error,ex.what(),n);error[n]='\0';}
    }catch(...){
        if(capacity){const char* msg="scene parsing failed";std::size_t n=std::min(capacity-1,std::strlen(msg));std::memcpy(error,msg,n);error[n]='\0';}
    }
    return nullptr;
}
extern "C" mv_scene_view mv_scene_get_view(const mv_scene* scene){
    if(!scene)return {0,0,nullptr,0};
    return {scene->width,scene->height,scene->views.data(),scene->views.size()};
}
extern "C" void mv_scene_destroy(mv_scene* scene){delete scene;}
