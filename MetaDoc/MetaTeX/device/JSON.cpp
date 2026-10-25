#include "MetaTeX.h"
#include "Vector.h"
#include <sstream>
namespace metatex {
namespace {
std::string quoted(std::string_view s){
    std::string r="\"";const char* digits="0123456789abcdef";
    for(unsigned char c:s){
        switch(c){case '"':r+="\\\"";break;case '\\':r+="\\\\";break;case '\n':r+="\\n";break;
            case '\r':r+="\\r";break;case '\t':r+="\\t";break;
            default:if(c<32){r+="\\u00";r+=digits[c>>4];r+=digits[c&15];}else r+=char(c);}
    }
    return r+'"';
}
std::string blockName(BlockKind k){
    switch(k){case BlockKind::Paragraph:return "paragraph";case BlockKind::Heading:return "heading";
    case BlockKind::ListItem:return "list-item";case BlockKind::Quote:return "quote";
    case BlockKind::Code:return "code";case BlockKind::DisplayMath:return "display-math";
    case BlockKind::HorizontalRule:return "horizontal-rule";case BlockKind::Note:return "note";
    case BlockKind::TableOfContents:return "table-of-contents";case BlockKind::PageBreak:return "page-break";
    case BlockKind::VectorGraphic:return "vector-graphic";}
    return "unknown";
}
std::string spanName(SpanKind k){
    switch(k){case SpanKind::Text:return "text";case SpanKind::Bold:return "bold";
    case SpanKind::Italic:return "italic";case SpanKind::Code:return "code";
    case SpanKind::Math:return "math";case SpanKind::Link:return "link";
    case SpanKind::Image:return "image";case SpanKind::SmallCaps:return "smallcaps";
    case SpanKind::Underline:return "underline";}
    return "unknown";
}
}
std::string renderJSON(const Document& doc){
    auto o=detail::vectorStream();
    o<<"{\n  \"format\": \"metatex-document-v1\",\n  \"title\": "<<quoted(doc.title)
     <<",\n  \"author\": "<<quoted(doc.author)<<",\n  \"date\": "<<quoted(doc.date)
     <<",\n  \"blocks\": [\n";
    for(std::size_t i=0;i<doc.blocks.size();++i){
        const auto& b=doc.blocks[i];
        o<<"    {\"type\": "<<quoted(blockName(b.kind))<<", \"level\": "<<b.level
         <<", \"ordered\": "<<(b.ordered?"true":"false")<<", \"id\": "<<quoted(b.id)<<", \"spans\": [";
        for(std::size_t j=0;j<b.spans.size();++j){
            const auto& s=b.spans[j];
            if(j)o<<", ";
            o<<"{\"type\": "<<quoted(spanName(s.kind))<<", \"text\": "<<quoted(s.text)<<", \"extra\": "<<quoted(s.extra)<<"}";
        }
        o<<"]";
        if(b.kind==BlockKind::VectorGraphic){
            auto scene=detail::sceneOf(b);
            o<<", \"graphic\": {\"format\": \"metavect-scene-v1\", \"width\": "<<scene.width<<", \"height\": "<<scene.height
             <<", \"displayWidth\": "<<(b.graphicWidth>0?b.graphicWidth:scene.width)<<", \"paths\": [";
            auto paint=[&](mv_paint p){if(!p.enabled)o<<"null";else o<<'['<<p.red<<','<<p.green<<','<<p.blue<<']';};
            for(std::size_t k=0;k<scene.count;++k){const auto& p=scene.paths[k];if(k)o<<',';
                o<<"{\"stroke\":";paint(p.stroke);o<<",\"fill\":";paint(p.fill);o<<",\"strokeWidth\":"<<p.stroke_width<<",\"commands\":[";
                for(std::size_t n=0;n<p.count;++n){const auto& c=p.commands[n];if(n)o<<',';
                    o<<'['<<quoted(c.op==MV_MOVE?"M":c.op==MV_LINE?"L":c.op==MV_CUBIC?"C":"Z");
                    int count=c.op==MV_CUBIC?6:c.op==MV_CLOSE?0:2;
                    for(int v=0;v<count;++v)o<<','<<c.values[v];
                    o<<']';
                }o<<"]}";
            }o<<"]}";
        }
        o<<"}"<<(i+1<doc.blocks.size()?',':' ')<<"\n";
    }
    o<<"  ],\n  \"references\": {";bool first=true;
    const std::map<std::string,std::string> references(doc.references.begin(),doc.references.end());
    for(const auto& [k,v]:references){if(!first)o<<", ";o<<quoted(k)<<": "<<quoted(v);first=false;}
    o<<"},\n  \"diagnostics\": [";
    for(std::size_t i=0;i<doc.diagnostics.size();++i){const auto& d=doc.diagnostics[i];if(i)o<<", ";
        o<<"{\"severity\": "<<quoted(d.severity==Diagnostic::Severity::Error?"error":"warning")
         <<", \"file\": "<<quoted(d.file)<<", \"line\": "<<d.line
         <<", \"column\": "<<d.column<<", \"message\": "<<quoted(d.message)<<"}";
    }
    o<<"]\n}\n";return o.str();
}
}
