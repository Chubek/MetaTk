#pragma once
#include "MetaTeX.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace metatex::detail {
inline mv_scene_view sceneOf(const Block& block){
    if(!block.graphic)throw std::runtime_error("vector block has no scene");
    if(!std::isfinite(block.graphicWidth)||block.graphicWidth<0||(block.graphicWidth>0&&block.graphicWidth<0.01)||block.graphicWidth>1000000)
        throw std::runtime_error("invalid vector block width");
    return block.graphic->view();
}
inline std::ostringstream vectorStream(){
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(12);return out;
}
inline double graphicScale(const Block& block,double maxWidth,double maxHeight){
    auto scene=sceneOf(block);
    double requested=block.graphicWidth>0?block.graphicWidth/scene.width:1;
    return std::min({requested,maxWidth/scene.width,maxHeight/scene.height});
}
inline std::string svgPaint(mv_paint p){
    if(!p.enabled)return "none";
    auto out=vectorStream();out<<"rgb("<<p.red*100<<"%,"<<p.green*100<<"%,"<<p.blue*100<<"%)";return out.str();
}
inline std::string svgPaths(mv_scene_view scene){
    auto out=vectorStream();
    for(std::size_t i=0;i<scene.count;++i){const auto& path=scene.paths[i];
        out<<"<path fill=\""<<svgPaint(path.fill)<<"\" stroke=\""<<svgPaint(path.stroke)
           <<"\" stroke-width=\""<<path.stroke_width<<"\" stroke-miterlimit=\"10\" d=\"";
        for(std::size_t j=0;j<path.count;++j){const auto& c=path.commands[j];
            out<<(c.op==MV_MOVE?"M":c.op==MV_LINE?"L":c.op==MV_CUBIC?"C":"Z");
            int n=c.op==MV_CUBIC?6:c.op==MV_CLOSE?0:2;
            for(int k=0;k<n;++k)out<<' '<<c.values[k];
            out<<' ';
        }
        out<<"\"/>";
    }return out.str();
}
inline std::string pdfPaths(mv_scene_view scene){
    auto out=vectorStream();
    // PDF numbers do not permit exponent notation.
    out<<std::fixed<<std::setprecision(8);
    for(std::size_t i=0;i<scene.count;++i){const auto& p=scene.paths[i];
        out<<p.stroke_width<<" w\n";
        if(p.stroke.enabled)out<<p.stroke.red<<' '<<p.stroke.green<<' '<<p.stroke.blue<<" RG\n";
        if(p.fill.enabled)out<<p.fill.red<<' '<<p.fill.green<<' '<<p.fill.blue<<" rg\n";
        for(std::size_t j=0;j<p.count;++j){const auto& c=p.commands[j];
            int n=c.op==MV_CUBIC?6:c.op==MV_CLOSE?0:2;
            for(int k=0;k<n;++k)out<<c.values[k]<<' ';
            out<<(c.op==MV_MOVE?"m":c.op==MV_LINE?"l":c.op==MV_CUBIC?"c":"h")<<'\n';
        }
        out<<(p.fill.enabled?(p.stroke.enabled?"B":"f"):(p.stroke.enabled?"S":"n"))<<'\n';
    }return out.str();
}
}
