#include "MetaTeX.h"
#include <filesystem>
#include <iostream>
#include <locale>
#include <stdexcept>
extern "C" void metatex_register_plugin(metatex::Engine*);
using namespace metatex;
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed at line "+std::to_string(__LINE__)+": " #x);} while(false)
struct CommaLocale:std::numpunct<char>{char do_decimal_point() const override{return ',';}};
int main(int argc,char** argv){
    CHECK(argc==2);std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
    const std::string scene="metavect-scene 1 200 100; stroke 0 0 1; fill 1 0 0; width 2; rect 10 20 30 40; fill none; circle 100 50 20; path M 0 0 C 10 20 30 40 50 60 L 80 90 Z;";
    writeFile(dir/"sample.mvs",scene);
    Engine e;metatex_register_plugin(&e);
    for(const auto& source:{"Before\\metavect[width=100]{"+scene+"}{A < B & C}After",std::string(R"(Before\metavectfile[width=100]{sample.mvs}{A < B & C}After)")}){
        auto d=e.compile(source,dir/"main.tex");CHECK(d.diagnostics.empty());CHECK(d.blocks.size()==3);
        const auto& b=d.blocks[1];CHECK(b.kind==BlockKind::VectorGraphic);CHECK(b.graphicWidth==100);
        auto view=b.graphic->view();CHECK(view.width==200&&view.height==100&&view.count==3);
        CHECK(view.paths[0].count==5);CHECK(view.paths[0].commands[1].values[0]==40);
        CHECK(view.paths[1].commands[1].op==MV_CUBIC);CHECK(view.paths[2].commands[1].values[5]==60);
        auto html=renderHTML(d),pdf=renderPDF(d),json=renderJSON(d);
        CHECK(html.find("viewBox=\"0 0 200 100\"")!=std::string::npos);
        CHECK(html.find("width:100pt")!=std::string::npos);
        CHECK(html.find("A &lt; B &amp; C")!=std::string::npos);
        CHECK(html.find("M 10 20 L 40 20 L 40 60 L 10 60 Z")!=std::string::npos);
        CHECK(pdf.find("0.50000000 0 0 -0.50000000")!=std::string::npos);
        CHECK(pdf.find("10.00000000 20.00000000 m")!=std::string::npos);
        CHECK(pdf.find("1.00000000 0.00000000 0.00000000 rg")!=std::string::npos);
        CHECK(pdf.find("B\n")!=std::string::npos&&pdf.find("/Subtype /Image")==std::string::npos);
        CHECK(json.find("\"type\": \"vector-graphic\"")!=std::string::npos);
        CHECK(json.find("[\"C\",10,20,30,40,50,60]")!=std::string::npos);
        auto old=std::locale::global(std::locale(std::locale::classic(),new CommaLocale));
        CHECK(renderHTML(d)==html&&renderPDF(d)==pdf&&renderJSON(d)==json);
        metavect::Scene localized("metavect-scene 1 10.5 20.5;");CHECK(localized.view().width==10.5);
        std::locale::global(old);
        CHECK(renderText(d)=="Before\n\nA < B & C\n\nAfter\n\n");
        // Scene storage remains valid after compiling the next document.
        e.compile("Next");CHECK(renderHTML(d)==html);
        writeFile(dir/"fallback.pdf",pdf);
        if(renderCairoPDF(d,dir/"cairo.pdf"))CHECK(readFile(dir/"cairo.pdf").rfind("%PDF-1.4",0)==0);
    }
    for(const auto& source:{R"(\metavect{metavect-scene 1 0 100;}{bad})",R"(\metavect[width=nan]{metavect-scene 1 100 100;}{bad})",R"(\metavectfile{missing.mvs}{bad})",R"(\metavectfile{source.mvct}{bad})"}){
        auto d=e.compile(source,dir/"main.tex");CHECK(!d.diagnostics.empty());CHECK(d.blocks.empty());
    }
    auto d=e.compile(R"(\metavect{metavect-scene 1 200 100;}{Empty})");CHECK(d.blocks.size()==1);CHECK(d.blocks[0].graphic->view().count==0);
    std::cout<<"Vector renderer tests passed\n";
}
