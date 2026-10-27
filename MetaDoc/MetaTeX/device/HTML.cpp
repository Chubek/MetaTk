#include "MetaTeX.h"
#include "Vector.h"
#include <algorithm>
#include <sstream>
namespace metatex {
namespace {
std::string html(std::string_view v){std::string o;for(char c:v){switch(c){case '&':o+="&amp;";break;case '<':o+="&lt;";break;case '>':o+="&gt;";break;case '"':o+="&quot;";break;case '\'':o+="&#39;";break;default:o+=c;}}return o;}
std::string spans(const Document& doc,const std::vector<Span>& v){
    std::string o;
    for(const auto& s:v){std::string t=html(s.text);std::replace(t.begin(),t.end(),'\n',' ');
        switch(s.kind){
        case SpanKind::Text:o+=t;break;
        case SpanKind::Bold:o+="<strong>"+t+"</strong>";break;
        case SpanKind::Italic:o+="<em>"+t+"</em>";break;
        case SpanKind::Code:o+="<code>"+t+"</code>";break;
        case SpanKind::SmallCaps:o+="<span class=\"smallcaps\">"+t+"</span>";break;
        case SpanKind::Underline:o+="<u>"+t+"</u>";break;
        case SpanKind::Math:o+=renderMathML(s.text);break;
        case SpanKind::Link: {
            if(s.extra.rfind("ref:",0)==0){auto it=doc.references.find(s.extra.substr(4));o+="<span class=\"ref\">"+(it==doc.references.end()?"??":html(it->second))+"</span>";}
            else if(s.extra.rfind("https://",0)==0||s.extra.rfind("http://",0)==0||(!s.extra.empty()&&s.extra[0]=='#'))o+="<a href=\""+html(s.extra)+"\" rel=\"noreferrer\">"+t+"</a>";
            else o+=t;
            break;}
        case SpanKind::Image:o+="<figure><img src=\""+html(s.extra)+"\" alt=\""+t+"\" loading=\"lazy\"></figure>";break;
        }
    }
    return o;
}
}
std::string renderHTML(const Document& doc){
    auto out=detail::vectorStream();
    out<<"<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
          "<title>"<<html(doc.title.empty()?"MetaTeX document":doc.title)<<"</title>"
          "<style>:root{color-scheme:light dark}body{max-width:48rem;margin:3rem auto;padding:0 1.25rem;font:17px/1.65 Georgia,serif}"
          "h1,h2,h3,h4{font-family:system-ui,sans-serif;line-height:1.25;margin-top:1.7em}"
          "pre,code{font-family:ui-monospace,monospace}pre{white-space:pre-wrap;padding:1rem;border-radius:7px;background:rgba(128,128,128,.12)}"
          "math{font-size:1.14em} .display-math{text-align:center;overflow:auto;padding:1em}"
          ".note{border-left:4px solid #7184bd;padding:.5em 1em;background:rgba(128,128,128,.1)}"
          ".smallcaps{font-variant:small-caps}.list-item{margin:.25rem 0}blockquote{border-left:3px solid #aaa;margin-left:0;padding-left:1rem}"
          "a{color:inherit;text-underline-offset:3px}</style></head><body>\n";
    for(const auto& b:doc.blocks){std::string t=spans(doc,b.spans);
        switch(b.kind){
        case BlockKind::VectorGraphic:{
            auto scene=detail::sceneOf(b);auto geometry=detail::vectorStream();
            geometry<<"<figure class=\"metavect\"><svg xmlns=\"http://www.w3.org/2000/svg\" role=\"img\" aria-label=\""
                    <<html(b.spans.empty()?"Vector graphic":b.spans.front().text)<<"\" viewBox=\"0 0 "<<scene.width<<' '<<scene.height
                    <<"\" style=\"display:block;max-width:100%;height:auto;overflow:hidden;width:"
                    <<(b.graphicWidth>0?b.graphicWidth:scene.width)<<"pt\"><title>"
                    <<html(b.spans.empty()?"Vector graphic":b.spans.front().text)<<"</title>"<<detail::svgPaths(scene)
                    <<"</svg><figcaption>"<<t<<"</figcaption></figure>\n";
            out<<geometry.str();break;}
        case BlockKind::Heading:{int h=std::clamp(b.level+1,1,6);if(b.level==0)h=1;
            out<<"<h"<<h<<" id=\""<<html(b.id)<<"\">"<<t<<"</h"<<h<<">\n";break;}
        case BlockKind::Paragraph:out<<"<p>"<<t<<"</p>\n";break;
        case BlockKind::ListItem:out<<"<div class=\"list-item\" style=\"margin-left:"<<1.4*b.level<<"rem\">"<<(b.ordered?std::to_string(std::max(1,b.listNumber))+". ":"• ")<<t<<"</div>\n";break;
        case BlockKind::Quote:out<<"<blockquote>"<<t<<"</blockquote>\n";break;
        case BlockKind::Code:
            out<<"<pre><code";if(!b.language.empty())out<<" data-language=\""<<html(b.language)<<"\"";
            out<<">";for(const auto& span:b.spans)out<<html(span.text);out<<"</code></pre>\n";break;
        case BlockKind::DisplayMath:out<<"<div class=\"display-math\">"<<t<<"</div>\n";break;
        case BlockKind::HorizontalRule:out<<"<hr>\n";break;
        case BlockKind::Note:out<<"<aside class=\"note\"";if(!b.role.empty())out<<" data-kind=\""<<html(b.role)<<"\"";out<<">"<<t<<"</aside>\n";break;
        case BlockKind::PageBreak:out<<"<div style=\"break-before:page\"></div>\n";break;
        case BlockKind::TableOfContents:
            out<<"<nav aria-label=\"Table of contents\"><strong>Contents</strong><ol>";
            for(const auto& h:doc.blocks)if(h.kind==BlockKind::Heading && h.level>0)out<<"<li style=\"margin-left:"<<(h.level-1)<<"em\"><a href=\"#"<<html(h.id)<<"\">"<<spans(doc,h.spans)<<"</a></li>";
            out<<"</ol></nav>\n";break;
        }
    }
    return out.str()+"</body></html>\n";
}
std::string renderText(const Document& doc){
    std::string out;
    for(const auto& b:doc.blocks){
        if(b.kind==BlockKind::Heading)out+=std::string(std::max(0,b.level),'#')+" ";
        if(b.kind==BlockKind::ListItem)out+=std::string(std::max(0,b.level-1)*2,' ')+(b.ordered?std::to_string(std::max(1,b.listNumber))+". ":"- ");
        for(const auto& s:b.spans){
            if(s.kind==SpanKind::Link && s.extra.rfind("ref:",0)==0){auto i=doc.references.find(s.extra.substr(4));out+=i==doc.references.end()?"??":i->second;}
            else out+=s.text;
        }
        out+='\n';if(b.kind!=BlockKind::ListItem)out+='\n';
    }return out;
}
}
