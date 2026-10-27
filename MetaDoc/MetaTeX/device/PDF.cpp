#include "MetaTeX.h"
#include "Vector.h"
#include "../utils/Memory.hpp"
#include <hpdf.h>
#include <hpdf_doc.h>
#include <hpdf_pages.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
namespace metatex {
namespace {
std::string escapePDF(std::string_view s){
    std::string out;
    for(std::size_t i=0;i<s.size();++i){unsigned char c=(unsigned char)s[i];
        if(c=='('||c==')'||c=='\\'){out+='\\';out+=(char)c;}
        else if(c=='\n'||c=='\r')out+=' ';
        else if(c>=32 && c<127)out+=(char)c;
        else if(c>=0xC2){ // limited Latin-1 transcoding for PDF standard font
            if(c==0xC2 && i+1<s.size() && (unsigned char)s[i+1]>=0xA0){out+=s[++i];}
            else if(c==0xC3 && i+1<s.size()){unsigned char b=(unsigned char)s[++i];out+=(char)(b+0x40);}
            else {out+='?';while(i+1<s.size()&&((unsigned char)s[i+1]&0xC0)==0x80)++i;}
        } else out+='?';
    }return out;
}
std::string textOf(const Document& doc,const Block& b){
    std::string result;
    for(const auto& s:b.spans){
        if(s.kind==SpanKind::Link&&s.extra.rfind("ref:",0)==0){auto it=doc.references.find(s.extra.substr(4));result+=it==doc.references.end()?"??":it->second;}
        else if(s.kind==SpanKind::Image)result+="[image: "+s.extra+"]";
        else result+=s.text;
    }
    return result;
}
std::vector<std::string> wrap(const std::string& input,std::size_t width){
    std::vector<std::string> lines;std::string line,word;
    auto wordFlush=[&](){
        if(word.empty())return;
        if(!line.empty() && line.size()+word.size()+1>width){lines.push_back(line);line.clear();}
        if(!line.empty())line+=' ';
        while(word.size()>width){if(!line.empty()){lines.push_back(line);line.clear();}
            lines.push_back(word.substr(0,width));word.erase(0,width);}
        line+=word;word.clear();
    };
    for(char c:input){
        if(c=='\n'){wordFlush();lines.push_back(line);line.clear();}
        else if(c==' '||c=='\t')wordFlush();else word+=c;
    }
    wordFlush();if(!line.empty())lines.push_back(line);
    if(lines.empty())lines.push_back("");
    return lines;
}
struct PagePainter {
    std::vector<std::string> pages{std::string{}};
    double y=786;
    void addLine(std::string line,double font=11,bool bold=false,double leading=15,double left=63){
        if(y-leading<55){pages.push_back({});y=786;}
        auto c=detail::vectorStream();c<<std::fixed<<std::setprecision(2)
            <<"BT /"<<(bold?"F2":"F1")<<" "<<font<<" Tf "<<left<<" "<<y<<" Td ("<<escapePDF(line)<<") Tj ET\n";
        pages.back()+=c.str();y-=leading;
    }
    void gap(double distance){y-=distance;if(y<55){pages.push_back({});y=786;}}
};
}
std::string renderPDF(const Document& doc){
    PagePainter p;
    for(const auto& b:doc.blocks){
        if(b.kind==BlockKind::VectorGraphic){
            auto scene=detail::sceneOf(b);double scale=detail::graphicScale(b,467,700);
            double height=scene.height*scale;
            if(p.y-height<55){p.pages.push_back({});p.y=786;}
            auto out=detail::vectorStream();out<<std::fixed<<std::setprecision(8);
            out<<"q\n"<<scale<<" 0 0 "<<-scale<<" 64 "<<p.y<<" cm\n0 0 "<<scene.width<<' '<<scene.height<<" re W n\n"
               <<detail::pdfPaths(scene)<<"Q\n";
            p.pages.back()+=out.str();p.y-=height+15;
            for(const auto& line:wrap(textOf(doc,b),85))p.addLine(line,10,false,14);
            p.gap(9);continue;
        }
        if(b.kind==BlockKind::PageBreak){p.pages.push_back({});p.y=786;continue;}
        if(b.kind==BlockKind::HorizontalRule){p.addLine("____________________________________________________________",10);p.gap(9);continue;}
        if(b.kind==BlockKind::TableOfContents){p.addLine("Contents",15,true,23);
            for(const auto& s:doc.blocks)
            if(s.kind==BlockKind::Heading && s.level>0)p.addLine(textOf(doc,s),10,false,15,72+s.level*12);
            p.gap(10);continue;}
        std::string text=textOf(doc,b);double font=11,leading=16,left=64;bool bold=false;
        if(b.kind==BlockKind::Heading){font=b.level==0?22:b.level==1?18:b.level==2?15:13;
            leading=font*1.38;bold=true;p.gap(b.level==0?18:13);}
        else if(b.kind==BlockKind::DisplayMath){font=12;left=90;p.gap(9);}
        else if(b.kind==BlockKind::Code){font=10;leading=13;left=82;}
        else if(b.kind==BlockKind::Quote||b.kind==BlockKind::Note){left=87;}
        else if(b.kind==BlockKind::ListItem){text=(b.ordered?"1. ":"* ")+text;left+=b.level*16;}
        auto lines=wrap(text,static_cast<std::size_t>(std::max(25.0,(595.0-left-48)/(font*0.52))));
        for(const auto& line:lines)p.addLine(line,font,bold,leading,left);
        if(b.kind!=BlockKind::ListItem)p.gap(b.kind==BlockKind::Paragraph?9:6);
    }
    // libHaru owns PDF resources, objects, xrefs and serialization. Its low-level
    // page-stream interface retains the native renderer's exact vector geometry.
    // No exception crosses a C callback: failures are checked at the call site.
    HPDF_Doc pdf=HPDF_New(nullptr,nullptr);
    if(!pdf)throw std::runtime_error("libHaru: document allocation failed");
    util::ScopeExit cleanup([&]{HPDF_Free(pdf);});
    auto check=[](HPDF_STATUS status){if(status!=HPDF_OK)throw std::runtime_error("libHaru error "+std::to_string(status));};
    pdf->pdf_version=HPDF_VER_14;
    check(HPDF_SetCompressionMode(pdf,HPDF_COMP_NONE));
    check(HPDF_SetInfoAttr(pdf,HPDF_INFO_CREATOR,"MetaTeX (libHaru)"));
    auto regular=HPDF_GetFont(pdf,"Helvetica","WinAnsiEncoding");
    auto bold=HPDF_GetFont(pdf,"Helvetica-Bold","WinAnsiEncoding");
    if(!regular||!bold)throw std::runtime_error("libHaru: cannot load base fonts");
    for(const auto& content:p.pages){
        auto page=HPDF_AddPage(pdf);
        if(!page)throw std::runtime_error("libHaru: cannot allocate page");
        check(HPDF_Page_SetWidth(page,595));check(HPDF_Page_SetHeight(page,842));
        const char* f1=HPDF_Page_GetLocalFontName(page,regular);
        const char* f2=HPDF_Page_GetLocalFontName(page,bold);
        if(!f1||!f2||std::string_view(f1)!="F1"||std::string_view(f2)!="F2")
            throw std::runtime_error("libHaru: unexpected font resource names");
        auto attr=static_cast<HPDF_PageAttr>(page->attr);
        check(HPDF_Stream_WriteStr(attr->stream,content.c_str()));
    }
    check(HPDF_SaveToStream(pdf));
    HPDF_UINT32 size=HPDF_GetStreamSize(pdf);std::string result(size,'\0');
    check(HPDF_ReadFromStream(pdf,reinterpret_cast<HPDF_BYTE*>(result.data()),&size));
    result.resize(size);return result;
}
}
