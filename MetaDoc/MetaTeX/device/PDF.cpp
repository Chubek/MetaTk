#include "MetaTeX.h"
#include "Vector.h"
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
    // 1: catalog; 2: pages; 3: regular font; 4: bold font;
    // for page k: (5+2k): page, (6+2k): stream.
    int count=4+2*(int)p.pages.size();
    std::vector<std::string> objects(count+1);
    objects[1]="<< /Type /Catalog /Pages 2 0 R >>";
    auto pages=detail::vectorStream();
    pages<<"<< /Type /Pages /Kids [";
    for(std::size_t i=0;i<p.pages.size();++i)pages<<(5+2*i)<<" 0 R ";
    pages<<"] /Count "<<p.pages.size()<<" >>";objects[2]=pages.str();
    objects[3]="<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
    objects[4]="<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>";
    for(std::size_t i=0;i<p.pages.size();++i){
        std::size_t pageId=5+i*2, streamId=6+i*2;
        objects[pageId]="<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> /Contents "+std::to_string(streamId)+" 0 R >>";
        const auto& content=p.pages[i];
        objects[streamId]="<< /Length "+std::to_string(content.size())+" >>\nstream\n"+content+"endstream";
    }
    std::string result="%PDF-1.4\n%MetaTeX\n";std::vector<std::size_t> offsets(count+1);
    for(int i=1;i<=count;i++){offsets[i]=result.size();result+=std::to_string(i)+" 0 obj\n"+objects[i]+"\nendobj\n";}
    std::size_t xref=result.size();
    auto trailer=detail::vectorStream();trailer<<"xref\n0 "<<count+1<<"\n0000000000 65535 f \n";
    for(int i=1;i<=count;i++)trailer<<std::setw(10)<<std::setfill('0')<<offsets[i]<<" 00000 n \n";
    trailer<<"trailer\n<< /Size "<<count+1<<" /Root 1 0 R >>\nstartxref\n"<<xref<<"\n%%EOF\n";
    return result+trailer.str();
}
}
