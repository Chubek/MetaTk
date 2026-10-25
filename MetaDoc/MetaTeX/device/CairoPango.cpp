#include "MetaTeX.h"
#include "Vector.h"
#ifdef METATEX_HAVE_PANGOCAIRO
#include <cairo.h>
#include <cairo-pdf.h>
#include <pango/pangocairo.h>
#include <algorithm>
#include <stdexcept>
namespace metatex {
namespace {
std::string blockText(const Document& doc,const Block& b){
    std::string text;
    for(const auto& s:b.spans){
        if(s.kind==SpanKind::Link && s.extra.rfind("ref:",0)==0){auto it=doc.references.find(s.extra.substr(4));text+=it==doc.references.end()?"??":it->second;}
        else if(s.kind==SpanKind::Image)text+="[image: "+s.extra+"]";
        else text+=s.text;
    }return text;
}
}
bool renderCairoPDF(const Document& doc,const std::filesystem::path& output){
    // PangoCairo delegates Unicode shaping and font selection to HarfBuzz,
    // FreeType and Fontconfig. cairo-pdf embeds the selected scalable fonts.
    const double width=595.0,height=842.0,margin=62.0;
    cairo_surface_t* surface=cairo_pdf_surface_create(output.c_str(),width,height);
    std::unique_ptr<cairo_surface_t,decltype(&cairo_surface_destroy)> surfaceOwner(surface,cairo_surface_destroy);
    if(cairo_surface_status(surface)!=CAIRO_STATUS_SUCCESS)throw std::runtime_error("unable to create cairo PDF surface");
    cairo_pdf_surface_restrict_to_version(surface,CAIRO_PDF_VERSION_1_4);
    cairo_t* cr=cairo_create(surface);
    std::unique_ptr<cairo_t,decltype(&cairo_destroy)> contextOwner(cr,cairo_destroy);
    if(cairo_status(cr)!=CAIRO_STATUS_SUCCESS)throw std::runtime_error("unable to create cairo context");
    double y=margin;
    for(const auto& block:doc.blocks){
        if(block.kind==BlockKind::VectorGraphic){
            auto scene=detail::sceneOf(block);double scale=detail::graphicScale(block,width-2*margin,700);
            double h=scene.height*scale;
            if(y+h>height-margin){cairo_show_page(cr);y=margin;}
            cairo_save(cr);cairo_translate(cr,margin,y);cairo_scale(cr,scale,scale);
            cairo_rectangle(cr,0,0,scene.width,scene.height);cairo_clip(cr);
            for(std::size_t i=0;i<scene.count;++i){const auto& p=scene.paths[i];
                cairo_new_path(cr);cairo_set_line_width(cr,p.stroke_width);
                for(std::size_t j=0;j<p.count;++j){const auto& c=p.commands[j];const auto* v=c.values;
                    switch(c.op){case MV_MOVE:cairo_move_to(cr,v[0],v[1]);break;
                    case MV_LINE:cairo_line_to(cr,v[0],v[1]);break;
                    case MV_CUBIC:cairo_curve_to(cr,v[0],v[1],v[2],v[3],v[4],v[5]);break;
                    case MV_CLOSE:cairo_close_path(cr);break;}
                }
                if(p.fill.enabled){cairo_set_source_rgb(cr,p.fill.red,p.fill.green,p.fill.blue);cairo_fill_preserve(cr);}
                if(p.stroke.enabled){cairo_set_source_rgb(cr,p.stroke.red,p.stroke.green,p.stroke.blue);cairo_stroke(cr);}
                cairo_new_path(cr);
            }
            cairo_restore(cr);y+=h+16;
            // Caption follows through the normal Unicode text layout below.
        }
        if(block.kind==BlockKind::PageBreak){cairo_show_page(cr);y=margin;continue;}
        if(block.kind==BlockKind::HorizontalRule){
            if(y>height-margin-20){cairo_show_page(cr);y=margin;}
            cairo_move_to(cr,margin,y);cairo_line_to(cr,width-margin,y);cairo_set_line_width(cr,0.6);cairo_stroke(cr);y+=15;continue;
        }
        std::string text=blockText(doc,block);
        if(block.kind==BlockKind::ListItem)text=(block.ordered?"1. ":"• ")+text;
        if(block.kind==BlockKind::TableOfContents)text="Contents";
        if(text.empty())continue;
        int pt=11;
        if(block.kind==BlockKind::Heading)pt=block.level==0?23:block.level==1?18:block.level==2?15:13;
        if(block.kind==BlockKind::Code)pt=10;
        std::string face=(block.kind==BlockKind::Code?"Monospace":"Serif");
        face+=" "+std::to_string(pt);
        if(block.kind==BlockKind::Heading)face+=" Bold";
        PangoFontDescription* font=pango_font_description_from_string(face.c_str());
        std::unique_ptr<PangoFontDescription,decltype(&pango_font_description_free)> fontOwner(font,pango_font_description_free);
        PangoLayout* layout=pango_cairo_create_layout(cr);
        std::unique_ptr<PangoLayout,decltype(&g_object_unref)> layoutOwner(layout,g_object_unref);
        pango_layout_set_text(layout,text.c_str(),(int)text.size());
        pango_layout_set_font_description(layout,font);
        double left=margin+((block.kind==BlockKind::Quote||block.kind==BlockKind::Note)?24:0)
                  +((block.kind==BlockKind::ListItem)?block.level*14:0);
        pango_layout_set_width(layout,static_cast<int>((width-left-margin)*PANGO_SCALE));
        pango_layout_set_wrap(layout,PANGO_WRAP_WORD_CHAR);
        const int lines=pango_layout_get_line_count(layout);
        const double lineHeight=pt*1.7;
        double extra=(block.kind==BlockKind::Heading?17:block.kind==BlockKind::Paragraph?9:5);
        for(int n=0;n<lines;++n){
            if(y+lineHeight>height-margin){cairo_show_page(cr);y=margin;}
            PangoLayoutLine* line=pango_layout_get_line_readonly(layout,n);
            cairo_move_to(cr,left,y);
            pango_cairo_show_layout_line(cr,line);
            y+=lineHeight;
        }
        y+=extra;
    }
    cairo_show_page(cr);auto contextStatus=cairo_status(cr);contextOwner.reset();cairo_surface_finish(surface);
    if(contextStatus!=CAIRO_STATUS_SUCCESS||cairo_surface_status(surface)!=CAIRO_STATUS_SUCCESS)
        throw std::runtime_error("cairo PDF rendering failed");
    return true;
}
}
#else
namespace metatex {
bool renderCairoPDF(const Document&,const std::filesystem::path&){return false;}
}
#endif
