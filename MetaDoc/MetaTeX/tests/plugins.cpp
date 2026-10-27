#include "MetaTeX.hpp"
#include <dynalo/dynalo.hpp>
#include <iostream>
#include <stdexcept>

using namespace metatex;
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed at line "+std::to_string(__LINE__)+": " #x);} while(false)
std::string text(const Block& block){std::string result;for(const auto& span:block.spans)result+=span.text;return result;}
void contains(const std::string& value,const std::string& expected){
    if(value.find(expected)==std::string::npos)throw std::runtime_error("missing: "+expected+"\nin: "+value.substr(0,600));
}
std::string pdfEscape(std::string value){std::string result;for(char c:value){if(c=='('||c==')'||c=='\\')result+='\\';result+=c;}return result;}
void output(const Document& doc,const std::string& plain,const std::string& html){
    CHECK(!hasErrors(doc));contains(renderText(doc),plain);contains(renderHTML(doc),html);
    auto pdf=renderPDF(doc);CHECK(pdf.starts_with("%PDF-1.4"));contains(pdf,"("+pdfEscape(plain)+") Tj");
    contains(renderJSON(doc),"\"format\": \"metatex-document-v1\"");
}
int main(int argc,char** argv){
    CHECK(argc==5);
    // The real plugin entry points and library lifetimes are exercised together.
    dynalo::library latex(argv[1]),neko(argv[2]),vector(argv[3]);Engine e;
    for(auto* library:{&latex,&neko,&vector})library->get_function<void(Engine*)>("metatex_register_plugin")(&e);
    std::filesystem::path dir=argv[4];std::filesystem::create_directories(dir);
    auto compile=[&](const std::string& source){auto d=e.compile(source,dir/"main.tex");
        for(const auto& diagnostic:d.diagnostics)if(diagnostic.severity==Diagnostic::Severity::Error)throw std::runtime_error(source+": "+diagnostic.message);
        return d;};

    auto d=compile(R"(\newenvironment{panel}[2][L]{#1/#2:}{!}\begin{panel}{R}body\end{panel})");
    CHECK(d.blocks.size()==1&&text(d.blocks[0])=="L/R:body!");output(d,"L/R:body!","<p>L/R:body!</p>");
    d=compile(R"(\newenvironment{panel}[1][L]{#1}{!}{\renewenvironment{panel}{B}{?}\begin{panel}C\end{panel}}\begin{panel}[R]D\end{panel})");
    CHECK(d.blocks.size()==2);output(d,"BC?","<p>BC?</p>");output(d,"RD!","<p>RD!</p>");
    CHECK(e.hasEnvironment("panel"));e.compile("");CHECK(!e.hasEnvironment("panel"));
    d=compile(R"(\newenvironment{scope}{A}{B}\begin{scope}\renewenvironment{scope}{X}{Y}C\end{scope})");
    output(d,"ACB","<p>ACB</p>");
    d=compile(R"(\newenvironment{boxed}[1][Title]{\begin{quote}\textbf{#1}: }{\end{quote}}\begin{boxed}Body.\end{boxed}After.)");
    CHECK(d.blocks.size()==2&&d.blocks[0].kind==BlockKind::Quote&&d.blocks[1].kind==BlockKind::Paragraph);
    output(d,"Title: Body.","<blockquote><strong>Title</strong>: Body.</blockquote>");output(d,"After.","<p>After.</p>");
    d=compile(R"(\newenvironment{scoped}{\bgroup\bfseries}{\egroup}\begin{scoped}Bold\end{scoped}Plain)");
    CHECK(d.blocks[0].spans[0].kind==SpanKind::Bold&&d.blocks[1].spans[0].kind==SpanKind::Text);
    output(d,"Bold","<strong>Bold</strong>");output(d,"Plain","<p>Plain</p>");
    CHECK(!hasErrors(e.compileFragment(R"(\newenvironment{kept}{A}{B})")));
    d=e.compileFragment(R"(\begin{kept}X\end{kept})");output(d,"AXB","<p>AXB</p>");
    e.compile("");CHECK(!e.hasEnvironment("kept"));
    CHECK(hasErrors(e.compile(R"({\newenvironment{local}{}{} }\begin{local}x\end{local})")));

    d=compile(R"(\section*{Preface}\section[short]{First}\label{s}\subsection*{Aside}\subsection{Detail}\label{d}Refs \ref{s}/\ref{d})");
    CHECK(d.references.at("s")=="1"&&d.references.at("d")=="1.1");
    CHECK(d.blocks[0].kind==BlockKind::Heading&&text(d.blocks[0])=="Preface");
    output(d,"Refs 1/1.1","Refs <span class=\"ref\">1</span>/<span class=\"ref\">1.1</span>");
    for(const auto& command:{"chapter","part","paragraph","subparagraph"}){
        d=compile("\\"+std::string(command)+"*{Title}");CHECK(d.blocks.size()==1&&text(d.blocks[0])=="Title");output(d,"Title",">Title</h");
    }
    d=compile(R"(\newcommand*{\pair}[1]{<#1>}\pair{OK})");output(d,"<OK>","&lt;OK&gt;");
    d=compile(R"(\newcounter{trial}[section]\section{One}\refstepcounter{trial}\label{t}\setcounter{trial}{4}\roman{trial}/\Roman{trial}/\alph{trial}/\Alph{trial}/\arabic{trial}/\ref{t})");
    CHECK(d.references.at("t")=="1.1");output(d,"iv/IV/d/D/4/1.1","iv/IV/d/D/4/<span class=\"ref\">1.1</span>");
    d=compile(R"(\newtheorem{thm}{Theorem}[section]\newtheorem{lemma}[thm]{Lemma}\newtheorem*{remark}{Remark}\section{One}\begin{thm}[Name]\label{a}Claim.\end{thm}\begin{lemma}\label{b}Next.\end{lemma}\section{Two}\begin{thm}\label{c}Reset.\end{thm}\begin{remark}No number.\end{remark})");
    CHECK(d.references.at("a")=="1.1"&&d.references.at("b")=="1.2"&&d.references.at("c")=="2.1");
    output(d,"Theorem 1.1 (Name). Claim.","<strong>Theorem 1.1 (Name). </strong>Claim.");
    output(d,"Lemma 1.2. Next.","<strong>Lemma 1.2. </strong>Next.");
    output(d,"Remark. No number.","<strong>Remark. </strong>No number.");
    d=compile(R"(\begin{proof}[Reason]A.\par B.\end{proof})");CHECK(d.blocks.size()==2&&d.blocks[1].kind==BlockKind::Quote);
    output(d,"B. QED","<blockquote>B. QED</blockquote>");
    for(const auto& env:{"equation","equation*","align","align*","gather","gather*","displaymath"}){
        d=compile("\\begin{"+std::string(env)+"}x^2\\label{eq}\\end{"+env+"}");
        CHECK(d.blocks.size()==1&&d.blocks[0].kind==BlockKind::DisplayMath&&text(d.blocks[0])=="x^2");
        output(d,"x^2","<msup>");
    }
    d=compile(R"(\begin{enumerate}\item One\item Two\begin{enumerate}\item Inner\end{enumerate}\item Three\end{enumerate})");
    CHECK(d.blocks.size()==4&&d.blocks[1].listNumber==2&&d.blocks[2].listNumber==1&&d.blocks[3].listNumber==3);
    output(d,"2. Two","2. Two</div>");output(d,"3. Three","3. Three</div>");
    contains(renderJSON(d),"\"listNumber\": 3");
    d=compile(R"(\begin{description}\item[Term]Meaning\end{description})");CHECK(d.blocks[0].kind==BlockKind::ListItem);contains(renderHTML(d),"<strong>Term</strong>");contains(renderPDF(d),"Term Meaning");
    d=compile(R"(\begin{figure}[ht]\caption[short]{Caption}\label{fig}\end{figure}Ref \ref{fig})");output(d,"Figure 1: Caption","Figure 1: Caption");CHECK(d.references.at("fig")=="1");

    d=compile("\\begin{verbatim}\n  a  % # \\x\n{b}\n\\end{verbatim}");
    CHECK(d.blocks.size()==1&&text(d.blocks[0])=="\n  a  % # \\x\n{b}\n");
    contains(renderHTML(d),"  a  % # \\x\n{b}");contains(renderPDF(d),"(  a  % # \\\\x) Tj");contains(renderPDF(d),"/Courier");
    d=compile(R"(\verb|a  % # \x|)");CHECK(text(d.blocks[0])==R"(a  % # \x)");contains(renderHTML(d),R"(<code>a  % # \x</code>)");contains(renderPDF(d),R"(a % # \\x)");

    for(const auto& role:{"note","tip","warning","important","solution","summary","callout"}){
        d=compile("\\"+std::string(role)+"[Title]{A\\par B}");
        CHECK(d.blocks.size()==2&&d.blocks[0].kind==BlockKind::Note&&d.blocks[1].kind==BlockKind::Note);
        CHECK(d.blocks[1].role==(std::string(role)=="callout"?"information":role));output(d,"Title: A","Title: A</aside>");output(d,"B",">B</aside>");
        d=compile("\\begin{"+std::string(role)+"}[Title]A\\par B\\end{"+role+"}");
        CHECK(d.blocks.size()==2&&d.blocks[1].kind==BlockKind::Note);output(d,"Title: A","Title: A</aside>");
    }
    d=compile(R"(\note{\def\local{bad}A\warning{B}C}D)");
    CHECK(d.blocks.size()==4&&d.blocks[2].role=="note"&&d.blocks[3].kind==BlockKind::Paragraph);CHECK(!e.hasCommand("local"));
    d=compile(R"(\component{Widget}{Content}\begin{component}{Panel}Body\end{component})");
    CHECK(d.blocks[0].role=="component");output(d,"Widget: Content","Widget: Content");output(d,"Panel: Body","Panel: Body");
    d=compile(R"(\exercise[First]{\label{ex}Try.}\begin{exercise}Again.\end{exercise}Ref \ref{ex})");
    CHECK(d.references.at("ex")=="1");output(d,"Exercise 1 (First): Try.","Exercise 1 (First): Try.");output(d,"Exercise 2: Again.","Exercise 2: Again.");
    d=compile(R"(\concept{A}\meta{B}\kbd{C  %}\inlinecode{D  #})");
    CHECK(d.blocks[0].spans.size()==4&&d.blocks[0].spans[0].kind==SpanKind::Bold&&d.blocks[0].spans[1].kind==SpanKind::SmallCaps);
    contains(renderHTML(d),"<code>C  %</code><code>D  #</code>");contains(renderPDF(d),"(ABC %D #) Tj");
    d=compile("\\codeblock[cpp]{  int x; % keep\n\t\\literal{a}\n}");
    CHECK(d.blocks.size()==1&&d.blocks[0].language=="cpp"&&text(d.blocks[0])=="  int x; % keep\n\t\\literal{a}\n");
    contains(renderHTML(d),"data-language=\"cpp\">  int x; % keep\n\t\\literal{a}\n");
    contains(renderJSON(d),"\"language\": \"cpp\"");contains(renderPDF(d),"(    \\\\literal{a}) Tj");
    d=compile("\\begin{codeblock}{python}\n  x = 1 # keep\n\\end{codeblock}");
    CHECK(d.blocks[0].language=="python");contains(renderHTML(d),"\n  x = 1 # keep\n");contains(renderPDF(d),"(  x = 1 # keep) Tj");
    writeFile(dir/"code.txt","  A\n% B\n");d=compile(R"(\codefile[text]{code.txt})");
    CHECK(text(d.blocks[0])=="  A\n% B\n");contains(renderHTML(d),"data-language=\"text\">  A\n% B\n");contains(renderPDF(d),"(  A) Tj");

    const std::string scene="metavect-scene 1 200 100; rect 0 0 200 100;";
    writeFile(dir/"scene.mvs",scene);
    for(const auto& option:{"height=25","width=80,height=25","scale=0.25","width=50.1875pt","width=50bp","width=0.6944444444444444in","width=1.763888888888889cm","width=17.63888888888889mm"}){
        d=compile("\\metavect["+std::string(option)+"]{"+scene+"}{Caption}");
        CHECK(d.blocks.size()==1&&std::abs(d.blocks[0].graphicWidth-50)<1e-9&&d.blocks[0].graphic->view().count==1);
        output(d,"Caption","width:50pt");contains(renderPDF(d),"0.25000000 0 0 -0.25000000");
    }
    for(const auto& command:{"metavectdef","metavectdeffile"}){
        auto source=std::string(command)=="metavectdef"?scene:"scene.mvs";
        d=compile("\\"+std::string(command)+"[height=25]{\\plot}{"+source+"}\\plot{One}\\plot[width=100]{Two}\\plot[]{Natural}");
        CHECK(d.blocks.size()==3&&d.blocks[0].graphicWidth==50&&d.blocks[1].graphicWidth==100&&d.blocks[2].graphicWidth==0);
        CHECK(d.blocks[0].graphic->view().paths==d.blocks[1].graphic->view().paths);
        output(d,"One","width:50pt");output(d,"Two","width:100pt");output(d,"Natural","width:200pt");
        CHECK(e.hasCommand("plot"));e.compile("Next");CHECK(!e.hasCommand("plot"));contains(renderHTML(d),"width:50pt");
    }
    d=compile("\\begin{metavect}[width=50]{Environment}\n"+scene+"\\end{metavect}");
    CHECK(d.blocks.size()==1&&d.blocks[0].graphicWidth==50);output(d,"Environment","width:50pt");

    for(const auto& source:{R"(\newcommand{\bad name}{x})",R"(\newcommand{\bad}[1]{#2})",R"(\newenvironment{bad}[10]{}{})",R"(\newenvironment{bad}[1]{#2}{})",R"(\renewenvironment{missing}{}{})",R"(\newtheorem{bad}[missing]{Bad})",R"(\begin{equation}x\end{align})",R"(\begin{unknown}x\end{unknown})",R"(\textbf{\egroup}OK)",R"(\codeblock{unfinished)",R"(\begin{verbatim}unfinished)",R"(\codefile{missing})",R"(\roman{missing})",R"(\newcounter{x}[missing})"}){
        CHECK(hasErrors(e.compile(source)));CHECK(!hasErrors(e.compile("Recovered")));
    }
    for(const auto& option:{"width=0","height=nan","scale=inf","width=1,width=2","bogus=2","scale=2,width=10","height=1e-200","width=2px","width=20,","scale=0"}){
        d=e.compile("\\metavect["+std::string(option)+"]{"+scene+"}{Bad}");CHECK(hasErrors(d)&&d.blocks.empty());
    }
    CHECK(hasErrors(e.compile("{\\metavectdef{\\plot}{"+scene+"}}\\plot{bad}")));
    CHECK(!hasErrors(e.compileFragment(R"(\metavectdeffile[scale=0.5]{\cached}{scene.mvs})",dir/"repl.tex")));
    std::filesystem::remove(dir/"scene.mvs");d=e.compileFragment(R"(\cached{Retained})");
    CHECK(d.blocks[0].graphicWidth==100);output(d,"Retained","width:100pt");
    for(const auto& source:{R"(\verb%a  # \x%)",R"(\verb*|a  # \x|)"}){
        d=compile(source);CHECK(text(d.blocks[0])==R"(a  # \x)");contains(renderHTML(d),R"(<code>a  # \x</code>)");contains(renderPDF(d),R"((a # \\x) Tj)");
    }
    d=compile(R"(\newcounter{n}\setcounter{n}{3}\then)");output(d,"3","<p>3</p>");
    d=compile(R"(\begin{table}[ht]\caption{Data}\label{tab}\end{table}\caption*{Unnumbered})");
    CHECK(d.references.at("tab")=="1");output(d,"Table 1: Data","Table 1: Data");output(d,"Figure: Unnumbered","Figure: Unnumbered");
    d=e.compile(R"(\note{\begin{warning}Bad}Recovered)");CHECK(hasErrors(d));
    CHECK(d.blocks.back().kind==BlockKind::Paragraph&&text(d.blocks.back())=="Recovered");
    CHECK(hasErrors(e.compile("\\verb|unterminated\nNext")));
    // Literal content keeps physical source locations, including CRLF and UTF-8.
    d=e.compile("\\codeblock{a\r\n\xCE\xB1\n}\n\\missing");CHECK(hasErrors(d));CHECK(d.diagnostics.back().line==4);
    // Pre-scanned macro text must not steal bytes from the caller's live input.
    d=compile(R"(\def\literal{\inlinecode{a}}\literal Tail)");
    CHECK(text(d.blocks[0])=="aTail");contains(renderHTML(d),"<code>a</code>Tail");contains(renderPDF(d),"(aTail) Tj");
    d=compile("\\codeblock{"+std::string(77,'A')+"\nB}");CHECK(renderPDF(d).find("() Tj")==std::string::npos);
    Document escaped;escaped.title="line\n\t\r\1\"\\";
    escaped.blocks.push_back(cpp::codeBlock(escaped.title));escaped.blocks[0].language=escaped.title;
    auto json=renderJSON(escaped);contains(json,R"(line\n\t\r\u0001\"\\)");CHECK(json.find('\1')==std::string::npos);
    d=compile(R"(\codeblock[x" data-injected="bad]{<script>})");
    contains(renderHTML(d),"data-language=\"x&quot; data-injected=&quot;bad\">&lt;script&gt;");
    for(const auto& runaway:{R"(\newenvironment{loop}{\begin{loop}}{}\begin{loop})",R"(\newcommand{\loop}{\note{\loop}}\loop)"}){
        bool bounded=false;try{e.compile(runaway);}catch(const std::runtime_error& error){bounded=std::string(error.what()).find("limit exceeded")!=std::string::npos;}
        CHECK(bounded);d=compile("Recovered");output(d,"Recovered","<p>Recovered</p>");
    }
    std::cout<<"Plugin contracts passed\n";
}
