#include "MetaTeX.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace metatex {
namespace {
std::string trim(std::string s) {
    auto blank=[](unsigned char c){return std::isspace(c);};
    while(!s.empty() && blank(s.front())) s.erase(s.begin());
    while(!s.empty() && blank(s.back())) s.pop_back();
    return s;
}
std::string number(int v) {return std::to_string(v);}
}
Engine::Engine(){scopes_.emplace_back();commandScopes_.emplace_back();environmentScopes_.emplace_back();registerScopes_.emplace_back();registerAliasScopes_.emplace_back();installPrimitives();installScripting();}
void Engine::registerCommand(std::string n, Handler h){expandablePrimitives_.erase(n);registerPrimitives_.erase(n);handlers_[std::move(n)]=std::move(h);}
void Engine::defineCommand(std::string n,Handler h){
    if(n.empty()||!h){report("invalid command definition",true);return;}
    scopes_.back().erase(n);registerAliasScopes_.back().erase(n);commandScopes_.back()[std::move(n)]=std::move(h);
}
void Engine::registerEnvironment(std::string n, Handler b, Handler e){environments_[std::move(n)]={std::move(b),std::move(e)};}
void Engine::defineEnvironment(std::string n,Handler b,Handler e){
    if(n.empty()||!b){report("invalid environment definition",true);return;}
    environmentScopes_.back()[std::move(n)]={std::move(b),std::move(e)};
}
void Engine::defineTokenEnvironment(std::string name,Macro begin,std::vector<Token> end){
    if(name.empty()){report("invalid environment definition",true);return;}
    if(begin.parameters<0||begin.parameters>9||(begin.optionalFirst&&begin.parameters==0)){
        report("invalid environment parameter count/default",true);return;
    }
    Environment env;
    env.begin=[begin=std::move(begin)](Engine& e){e.expand(begin);};
    env.endTokens=std::make_shared<const std::vector<Token>>(std::move(end));
    environmentScopes_.back()[std::move(name)]=std::move(env);
}
const Engine::Environment* Engine::lookupEnvironment(const std::string& n)const{
    for(auto i=environmentScopes_.rbegin();i!=environmentScopes_.rend();++i)
        if(auto found=i->find(n);found!=i->end())return &found->second;
    auto found=environments_.find(n);return found==environments_.end()?nullptr:&found->second;
}
bool Engine::hasEnvironment(const std::string& n)const{return lookupEnvironment(n)!=nullptr;}
void Engine::report(std::string message,bool error){document_.diagnostics.push_back({error?Diagnostic::Severity::Error:Diagnostic::Severity::Warning,std::move(message),file_.string(),current_.line,current_.column});}
Token Engine::pop(bool skipSpace) {
    while(ensureToken()) {
        if(++operations_>1500000)throw std::runtime_error("token processing budget exceeded (possible recursive macro)");
        Token t=std::move(stream_.front());stream_.pop_front();
        if(skipSpace && (t.kind==TokenKind::Space||t.kind==TokenKind::EndLine)) continue;
        current_=t;return t;
    }
    return {TokenKind::Sentinel,"",0,1,1};
}
Token Engine::peek(bool skipSpace) {
    if(skipSpace) while(ensureToken() && (stream_.front().kind==TokenKind::Space||stream_.front().kind==TokenKind::EndLine))pop();
    if(!ensureToken())return {TokenKind::Sentinel,""};
    return stream_.front();
}
bool Engine::ensureToken(){
    if(!stream_.empty())return true;
    if(!source_)return false;
    auto token=source_->next(catcodes_);
    if(token.kind==TokenKind::Sentinel)return false;
    token.directSource=true;stream_.push_back(std::move(token));return true;
}
bool Engine::consume(TokenKind kind){if(peek().kind==kind){pop();return true;}return false;}
std::vector<Token> Engine::readArgument(){
    Token t=pop(true);
    if(t.kind==TokenKind::Sentinel){report("missing required argument",true);return {};}
    if(t.kind==TokenKind::EndGroup){insert({t});report("missing argument before closing group",true);return {};}
    if(t.kind!=TokenKind::BeginGroup)return {t};
    int depth=1;std::vector<Token> result;
    while(depth){
        t=pop();
        if(t.kind==TokenKind::Sentinel){report("unclosed brace argument",true);break;}
        if(t.kind==TokenKind::BeginGroup)++depth;
        else if(t.kind==TokenKind::EndGroup)--depth;
        if(depth)result.push_back(std::move(t));
    }
    return result;
}
std::optional<std::vector<Token>> Engine::readOptionalArgument(){
    if(peek(true).value!="[")return std::nullopt;
    pop();int depth=0, braces=0;std::vector<Token> v;
    while(true){Token t=pop();if(t.kind==TokenKind::Sentinel){report("unclosed optional argument",true);break;}
        if(t.kind==TokenKind::BeginGroup)++braces;
        if(t.kind==TokenKind::EndGroup && braces>0)--braces;
        if(!braces && t.kind==TokenKind::Character && t.value=="[")++depth;
        if(!braces && t.kind==TokenKind::Character && t.value=="]") {if(depth==0)break;--depth;}
        v.push_back(std::move(t));}
    return v;
}
std::string Engine::tokensText(const std::vector<Token>& tokens) const {
    std::string s;
    for(std::size_t i=0;i<tokens.size();++i){const auto& t=tokens[i];
        if(t.kind==TokenKind::Control&&!t.active)s+='\\';
        s+=t.value;
        auto letter=[](unsigned char c){return std::isalpha(c)||c>=128;};
        if(t.kind==TokenKind::Control&&!t.active&&!t.value.empty()&&letter(t.value.back())&&i+1<tokens.size()&&
           tokens[i+1].kind==TokenKind::Character&&!tokens[i+1].value.empty()&&letter(tokens[i+1].value.front()))s+=' ';
    }
    return s;
}
std::string Engine::readTextArgument(){return trim(tokensText(readArgument()));}
std::string Engine::readOptionalText(std::string fallback){auto o=readOptionalArgument();return o?trim(tokensText(*o)):fallback;}
std::string Engine::readDelimitedText(){
    if(current_.directSource&&stream_.empty()&&source_){
        try{return source_->readLiteralDelimited();}catch(const std::exception& ex){report(ex.what(),true);return {};}
    }
    Token delim=pop();
    if(delim.kind==TokenKind::Character&&delim.value=="*")delim=pop();
    if(delim.kind==TokenKind::Sentinel){report("missing delimiter",true);return {};}
    if(delim.kind==TokenKind::Control||delim.kind==TokenKind::Space||delim.kind==TokenKind::EndLine){report("invalid literal delimiter",true);return {};}
    if(delim.directSource&&stream_.empty()&&source_){
        try{return source_->readLiteral(delim.value);}catch(const std::exception& ex){report(ex.what(),true);return {};}
    }
    std::string result;bool closed=false;
    while(ensureToken()){
        Token t=pop();if(t.kind==TokenKind::Sentinel)break;
        if(t.value==delim.value){closed=true;break;}
        if(t.kind==TokenKind::Control&&!t.active)result+='\\';
        result+=t.value;
    }
    if(!closed)report("unterminated delimited text",true);
    return result;
}
bool Engine::readStar(){auto t=peek(true);if(t.kind==TokenKind::Character&&t.value=="*"){pop();return true;}return false;}
std::string Engine::readLiteralArgument(){
    auto opening=pop(true);
    if(opening.kind!=TokenKind::BeginGroup){report("literal text requires a brace argument",true);return {};}
    if(opening.directSource&&stream_.empty()&&source_&&opening.value=="{"){
        try{return source_->readLiteral("}",true);}catch(const std::exception& ex){report(ex.what(),true);return {};}
    }
    insert({opening});return tokensText(readArgument());
}
std::string Engine::readLiteralEnvironment(const std::string& name){
    if(!current_.directSource||!stream_.empty()||!source_){report("literal environment requires direct source input",true);return {};}
    try{return source_->readLiteral("\\end{"+name+"}");}catch(const std::exception& ex){report(ex.what(),true);return {};}
}
void Engine::insert(std::vector<Token> tokens){
    if(tokens.size()>1500000-stream_.size())throw std::runtime_error("token queue limit exceeded");
    for(auto it=tokens.rbegin();it!=tokens.rend();++it){it->directSource=false;stream_.push_front(std::move(*it));}
}
void Engine::execute(const std::vector<Token>& tokens){
    executeTokens(tokens,scopes_.size(),envStack_.size());
}
void Engine::executeTokens(const std::vector<Token>& tokens,std::size_t localFloor,std::size_t environmentDepth){
    if(executionDepth_>=128)throw std::runtime_error("nested execution limit exceeded");
    // A scanner in this frame must never consume its caller's following tokens.
    auto outer=std::move(stream_);stream_.clear();++executionDepth_;
    auto outerSource=std::move(source_);source_.reset();
    auto floor=conditionalFloor_;conditionalFloor_=conditions_.size();
    auto groupFloor=groupFloor_;groupFloor_=localFloor;
    auto restoreGroups=[&](){
        if(envStack_.size()>environmentDepth){
            report("unclosed environment in executed argument: "+envStack_.back(),true);flush();
            if(blockContexts_.size()>envContextDepths_[environmentDepth])blockContexts_.resize(envContextDepths_[environmentDepth]);
            envStack_.resize(environmentDepth);envEnds_.resize(environmentDepth);envEndTokens_.resize(environmentDepth);envGroupDepths_.resize(environmentDepth);envItems_.resize(environmentDepth);
            envContextDepths_.resize(environmentDepth);
        }
        if(scopes_.size()>groupFloor_){report("unclosed group in executed argument",true);
            while(scopes_.size()>groupFloor_)endGroup();}
        groupFloor_=groupFloor;
    };
    try {insert(tokens);run(true);restoreGroups();}
    catch(...) {restoreGroups();stream_=std::move(outer);source_=std::move(outerSource);conditions_.resize(conditionalFloor_);conditionalFloor_=floor;--executionDepth_;throw;}
    stream_=std::move(outer);source_=std::move(outerSource);conditionalFloor_=floor;--executionDepth_;
}
void Engine::executeGrouped(const std::vector<Token>& tokens){
    beginGroup();
    try{execute(tokens);}catch(...){endGroup();throw;}
    endGroup();
}
void Engine::executeSource(std::string source){
    Lexer scanner(std::move(source));
    auto outer=std::move(stream_);stream_.clear();auto outerSource=std::move(source_);
    auto floor=conditionalFloor_;conditionalFloor_=conditions_.size();source_=std::move(scanner);
    try {run(true);}
    catch(...) {stream_=std::move(outer);source_=std::move(outerSource);conditions_.resize(conditionalFloor_);conditionalFloor_=floor;throw;}
    stream_=std::move(outer);source_=std::move(outerSource);conditionalFloor_=floor;
}
void Engine::beginGroup(){
    if(scopes_.size()>=1024)throw std::runtime_error("group nesting limit exceeded");
    scopes_.emplace_back();commandScopes_.emplace_back();environmentScopes_.emplace_back();groupStyles_.push_back(styles_);
    registerScopes_.emplace_back();registerAliasScopes_.emplace_back();
    catcodeSaves_.emplace_back();
    afterGroups_.emplace_back();
}
void Engine::endGroup(){
    if(scopes_.size()>groupFloor_){
        for(auto [character,code]:catcodeSaves_.back())catcodes_[character]=code;
        catcodeSaves_.pop_back();
        auto after=std::move(afterGroups_.back());afterGroups_.pop_back();
        scopes_.pop_back();commandScopes_.pop_back();environmentScopes_.pop_back();registerScopes_.pop_back();registerAliasScopes_.pop_back();styles_=std::move(groupStyles_.back());groupStyles_.pop_back();
        insert(std::move(after));
    }
    else report("unmatched closing group",true);
}
const Macro* Engine::lookup(const std::string& n) const {
    for(std::size_t i=scopes_.size();i-->0;) {
        auto m=scopes_[i].find(n);if(m!=scopes_[i].end())return &m->second;
        if(commandScopes_[i].contains(n))return nullptr;
    }return nullptr;
}
bool Engine::hasMacro(const std::string& n)const{return lookup(n)!=nullptr;}
bool Engine::hasCommand(const std::string& n)const{
    for(const auto& scope:commandScopes_)if(scope.contains(n))return true;
    return handlers_.contains(n)||lookup(n)!=nullptr;
}
std::vector<std::string> Engine::commandNames()const{
    std::vector<std::string> names;
    for(const auto& [name,handler]:handlers_)names.push_back(name);
    for(const auto& scope:scopes_)for(const auto& [name,macro]:scope)names.push_back(name);
    for(const auto& scope:commandScopes_)for(const auto& [name,handler]:scope)names.push_back(name);
    std::sort(names.begin(),names.end());names.erase(std::unique(names.begin(),names.end()),names.end());return names;
}
void Engine::defineMacro(std::string name,Macro m,bool global){
    if(name.empty()){report("empty macro name",true);return;}
    if(m.parameters<0||m.parameters>9){report("macro supports at most nine arguments",true);return;}
    if(m.optionalFirst&&m.parameters==0){report("optional macro parameter requires at least one argument",true);return;}
    if(m.replacement.size()>1500000)throw std::runtime_error("macro replacement limit exceeded");
    if(global){for(auto& scope:scopes_)scope.erase(name);for(auto& scope:commandScopes_)scope.erase(name);for(auto& scope:registerAliasScopes_)scope.erase(name);}
    else {commandScopes_.back().erase(name);registerAliasScopes_.back().erase(name);}
    (global?scopes_.front():scopes_.back())[std::move(name)]=std::move(m);
}
void Engine::expand(const Macro& macro){
    auto same=[](const Token& a,const Token& b){return a.kind==b.kind&&a.value==b.value&&a.active==b.active&&(a.kind!=TokenKind::Character||a.category==b.category);};
    for(const auto& expected:macro.prefix){
        if(!same(pop(),expected)){report("macro invocation does not match its parameter prefix",true);return;}
    }
    std::vector<std::vector<Token>> args;
    for(int i=0;i<macro.parameters;i++){
        if(i==0&&macro.optionalFirst){
            auto optional=readOptionalArgument();
            args.push_back(optional?std::move(*optional):*macro.optionalFirst);continue;
        }
        if(i>=static_cast<int>(macro.delimiters.size())||macro.delimiters[i].empty()){
            args.push_back(readArgument());continue;
        }
        const auto& delimiter=macro.delimiters[i];
        std::vector<Token> argument;
        // KMP matches overlapping delimiters in linear time. Group contents
        // cannot match a delimiter (TeX's macrocall scanner).
        std::vector<std::size_t> failure(delimiter.size());
        for(std::size_t j=1,k=0;j<delimiter.size();++j){
            while(k&&!same(delimiter[j],delimiter[k]))k=failure[k-1];
            if(same(delimiter[j],delimiter[k]))++k;
            failure[j]=k;
        }
        std::size_t matched=0;int depth=0;
        while(true){
            Token t=pop();
            if(t.kind==TokenKind::Sentinel){report("unterminated delimited macro argument",true);return;}
            argument.push_back(t);
            if(t.kind==TokenKind::BeginGroup&&depth==0&&same(t,delimiter[matched])&&matched+1==delimiter.size()){
                argument.resize(argument.size()-matched-1);break;
            }
            if(t.kind==TokenKind::BeginGroup){++depth;matched=0;continue;}
            if(t.kind==TokenKind::EndGroup){
                if(!depth){report("unexpected closing group in macro argument",true);return;}
                --depth;matched=0;continue;
            }
            if(depth)continue;
            while(matched&&!same(t,delimiter[matched]))matched=failure[matched-1];
            if(same(t,delimiter[matched]))++matched;
            if(matched==delimiter.size()){argument.resize(argument.size()-matched);break;}
        }
        // Strip braces only if the entire argument is one balanced group.
        if(!argument.empty()&&argument.front().kind==TokenKind::BeginGroup){
            int balance=0;std::size_t end=0;
            for(;end<argument.size();++end){
                if(argument[end].kind==TokenKind::BeginGroup)++balance;
                if(argument[end].kind==TokenKind::EndGroup)--balance;
                if(!balance)break;
            }
            if(end+1==argument.size()){argument.pop_back();argument.erase(argument.begin());}
        }
        args.push_back(std::move(argument));
    }
    std::vector<Token> expanded;
    for(std::size_t i=0;i<macro.replacement.size();i++){
        const Token& t=macro.replacement[i];
        if(t.kind==TokenKind::Parameter && !t.literalParameter && i+1<macro.replacement.size()){
            const auto& n=macro.replacement[++i];
            if(n.kind==TokenKind::Parameter){expanded.push_back(n);continue;}
            if(n.value.size()==1 && n.value[0]>='1' && n.value[0]<='9'){
                int index=n.value[0]-'1';
                if(index<(int)args.size()){
                    if(args[index].size()>1500000-expanded.size())throw std::runtime_error("macro expansion limit exceeded");
                    expanded.insert(expanded.end(),args[index].begin(),args[index].end());
                }
                else report("invalid macro argument reference",true);
                continue;
            }
            expanded.push_back(t);expanded.push_back(n);continue;
        }
        auto copy=t;copy.literalParameter=false;expanded.push_back(std::move(copy));
    }
    insert(std::move(expanded));
}
void Engine::scanDefinition(bool global,bool expandReplacement){
    global=global||globalAssignment_;
    Token name=pop(true);
    if(name.kind!=TokenKind::Control){report("\\def expects a control sequence",true);return;}
    Macro macro;bool valid=true;std::optional<Token> hashBrace;
    while(true){
        Token t=peek();
        if(t.kind==TokenKind::Sentinel){report("unterminated macro definition",true);return;}
        if(t.kind==TokenKind::BeginGroup)break;
        t=pop();
        if(t.kind==TokenKind::Parameter){
            Token d=pop();
            if(d.kind==TokenKind::BeginGroup){
                hashBrace=d;
                (macro.parameters?macro.delimiters.back():macro.prefix).push_back(d);
                insert({d});break;
            }
            if(d.kind==TokenKind::Character&&d.value==std::to_string(macro.parameters+1)&&macro.parameters<9){
                ++macro.parameters;macro.delimiters.emplace_back();
            }else{report("macro parameters must be numbered consecutively #1..#9",true);valid=false;}
        }else (macro.parameters?macro.delimiters.back():macro.prefix).push_back(std::move(t));
    }
    if(!expandReplacement)macro.replacement=readArgument();
    else {
        pop(); // Opening brace found by the parameter scanner.
        int depth=1;
        while(depth){
            Token t=expanded(false,true);
            if(t.kind==TokenKind::Sentinel){report("unclosed expanded definition",true);valid=false;break;}
            if(t.definitionOpaque){
                t.definitionOpaque=false;t.literalParameter=t.kind==TokenKind::Parameter;
                macro.replacement.push_back(std::move(t));continue;
            }
            if(t.kind==TokenKind::Control&&!t.expansionProtected&&!hasCommand(t.controlName())){
                report("undefined control sequence in expanded definition: \\"+t.value,true);valid=false;
            }
            t.expansionProtected=false;
            if(t.kind==TokenKind::BeginGroup)++depth;
            if(t.kind==TokenKind::EndGroup)--depth;
            if(depth)macro.replacement.push_back(std::move(t));
        }
    }
    if(hashBrace)macro.replacement.push_back(*hashBrace);
    for(std::size_t i=0;i<macro.replacement.size();++i){
        if(macro.replacement[i].kind!=TokenKind::Parameter||macro.replacement[i].literalParameter)continue;
        if(++i<macro.replacement.size()){
            const auto& t=macro.replacement[i];
            if(t.kind==TokenKind::Parameter)continue;
            if(t.kind==TokenKind::Character&&t.value.size()==1&&t.value[0]>='1'&&t.value[0]<='0'+macro.parameters)continue;
        }
        report("invalid parameter reference in macro replacement",true);valid=false;
    }
    if(valid)defineMacro(name.controlName(),std::move(macro),global);
}
void Engine::flush(){
    if(activeValid_){
        if(!active_.spans.empty() || active_.kind==BlockKind::HorizontalRule || active_.kind==BlockKind::TableOfContents || active_.kind==BlockKind::PageBreak)
            document_.blocks.push_back(std::move(active_));
        active_={};activeValid_=false;
    }
    pendingSpace_=false;successiveNewlines_=0;
}
void Engine::paragraph(){flush();}
void Engine::startBlock(BlockKind kind,int level,bool ordered){flush();active_={};active_.kind=kind;active_.level=level;active_.ordered=ordered;activeValid_=true;}
void Engine::beginBlockContext(BlockKind kind,std::string role){
    paragraph();blockContexts_.emplace_back(kind,std::move(role));
}
void Engine::endBlockContext(){paragraph();if(!blockContexts_.empty())blockContexts_.pop_back();}
void Engine::codeBlock(std::string text,std::string language){
    startBlock(BlockKind::Code);active_.language=std::move(language);
    addSpan(SpanKind::Code,std::move(text));paragraph();
}
void Engine::addGraphic(metavect::Scene scene,std::string caption,double width){
    if(!std::isfinite(width)||width<0||(width>0&&width<0.01)||width>1000000){report("invalid vector graphic width",true);return;}
    flush();Block block;block.kind=BlockKind::VectorGraphic;
    block.graphic=std::make_shared<const metavect::Scene>(std::move(scene));block.graphicWidth=width;
    block.spans.push_back({SpanKind::Text,std::move(caption),""});document_.blocks.push_back(std::move(block));
}
void Engine::addText(std::string text){
    if(text.empty())return;
    if(!activeValid_){active_={};if(!blockContexts_.empty()){active_.kind=blockContexts_.back().first;active_.role=blockContexts_.back().second;}activeValid_=true;}
    SpanKind kind=styles_.empty()?SpanKind::Text:styles_.back();
    if(pendingSpace_ && !active_.spans.empty() && !text.empty() && text[0]!='\n') {
        if(active_.spans.back().kind==kind&&active_.spans.back().extra.empty())active_.spans.back().text+=' ';
        else active_.spans.push_back({SpanKind::Text," ",""});
    }
    pendingSpace_=false;successiveNewlines_=0;
    if(!active_.spans.empty() && active_.spans.back().kind==kind && active_.spans.back().extra.empty())
        active_.spans.back().text+=text;
    else active_.spans.push_back({kind,std::move(text),""});
}
void Engine::addSpan(SpanKind kind,std::string text,std::string extra){
    if(!activeValid_){active_={};if(!blockContexts_.empty()){active_.kind=blockContexts_.back().first;active_.role=blockContexts_.back().second;}activeValid_=true;}
    if(pendingSpace_&&!active_.spans.empty())active_.spans.push_back({SpanKind::Text," ",""});
    pendingSpace_=false;successiveNewlines_=0;
    active_.spans.push_back({kind,std::move(text),std::move(extra)});
}
void Engine::addSpace(){if(activeValid_)pendingSpace_=true;}
void Engine::styledArgument(SpanKind kind){auto tokens=readArgument();beginGroup();styles_.push_back(kind);execute(tokens);endGroup();}
void Engine::heading(int level){heading(level,true);}
void Engine::heading(int level,bool numbered){
    std::vector<Token> argument=readArgument();
    flush();
    if(!numbered){} // Unnumbered headings do not replace the current reference value.
    else if(level==1){stepCounter("section");counters_["subsection"]=counters_["subsubsection"]=0;lastHeadingNumber_=number(counters_["section"]);}
    else if(level==2){stepCounter("subsection");counters_["subsubsection"]=0;lastHeadingNumber_=number(counters_["section"])+"."+number(counters_["subsection"]);}
    else if(level==3){stepCounter("subsubsection");lastHeadingNumber_=number(counters_["section"])+"."+number(counters_["subsection"])+"."+number(counters_["subsubsection"]);}
    else lastHeadingNumber_="";
    startBlock(BlockKind::Heading,level);lastHeadingId_="sec-"+std::to_string(++headingSequence_);
    active_.id=lastHeadingId_;
    executeGrouped(argument);flush();
}
void Engine::setTitle(std::string title){document_.title=std::move(title);}
void Engine::setAuthor(std::string author){document_.author=std::move(author);}
void Engine::setDate(std::string date){document_.date=std::move(date);}
void Engine::makeTitle(){
    if(!document_.title.empty()){
        startBlock(BlockKind::Heading,0);addText(document_.title);flush();
    }
    if(!document_.author.empty()||!document_.date.empty()){
        startBlock(BlockKind::Paragraph);addText(document_.author);
        if(!document_.date.empty())addText(" — "+document_.date);
        flush();
    }
}
void Engine::beginEnvironment(const std::string& name){
    auto found=lookupEnvironment(name);Environment env=found?*found:Environment{};
    beginGroup();envStack_.push_back(name);envEnds_.push_back(env.end);envEndTokens_.push_back(env.endTokens);
    envGroupDepths_.push_back(scopes_.size());envItems_.push_back(0);
    envContextDepths_.push_back(blockContexts_.size());
    if(env.begin)env.begin(*this);
    else report("unknown environment: "+name,true);
}
void Engine::endEnvironment(const std::string& name){
    auto found=std::find(envStack_.rbegin(),envStack_.rend(),name);
    if(found==envStack_.rend()){report("mismatched environment end: "+name,true);return;}
    auto index=envStack_.size()-1-std::distance(envStack_.rbegin(),found);
    auto ending=envEndTokens_[index];
    if(index+1!=envStack_.size()&&!ending){report("mismatched environment end: "+name,true);return;}
    if(envGroupDepths_[index]<=groupFloor_){report("unbalanced group in environment: "+name,true);return;}
    // A user-defined end body may close environments opened by its begin body.
    // It may not escape the defining environment's own group.
    if(ending)executeTokens(*ending,envGroupDepths_[index],index+1);
    if(scopes_.size()!=envGroupDepths_.back()||scopes_.size()<=groupFloor_){report("unbalanced group in environment: "+name,true);return;}
    auto handler=envEnds_.back();if(handler)handler(*this);
    flush();if(blockContexts_.size()>envContextDepths_.back())blockContexts_.resize(envContextDepths_.back());
    endGroup();envStack_.pop_back();envEnds_.pop_back();envEndTokens_.pop_back();envGroupDepths_.pop_back();envItems_.pop_back();envContextDepths_.pop_back();
}
void Engine::item(){
    auto isList=[](const auto& n){return n=="itemize"||n=="enumerate"||n=="description";};
    auto listIt=std::find_if(envStack_.rbegin(),envStack_.rend(),isList);
    if(listIt==envStack_.rend()){report("\\item outside a list",true);return;}
    int depth=(int)std::count_if(envStack_.begin(),envStack_.end(),isList);
    startBlock(BlockKind::ListItem,depth, *listIt=="enumerate");
    if(active_.ordered){active_.listNumber=++envItems_[envStack_.size()-1-std::distance(envStack_.rbegin(),listIt)];setReferenceValue(std::to_string(active_.listNumber));}
    if(auto opt=readOptionalArgument()) {addSpan(SpanKind::Bold,tokensText(*opt));addText(" ");}
}
void Engine::math(bool display,std::string terminator){
    std::vector<Token> tokens;bool ended=false;
    while(ensureToken()) {
        Token t=pop();
        if(t.kind==TokenKind::Sentinel)break;
        if(terminator=="$" && t.kind==TokenKind::MathShift){ended=true;break;}
        if(terminator=="$$" && t.kind==TokenKind::MathShift && peek().kind==TokenKind::MathShift){pop();ended=true;break;}
        if(t.kind==TokenKind::Control && t.value==terminator){ended=true;break;}
        tokens.push_back(std::move(t));
    }
    if(!ended)report("unterminated math expression",true);
    std::string expression=tokensText(tokens);
    if(display){startBlock(BlockKind::DisplayMath);addSpan(SpanKind::Math,std::move(expression));flush();}
    else addSpan(SpanKind::Math,std::move(expression));
}
void Engine::processMathShift(){bool display=peek().kind==TokenKind::MathShift;if(display)pop();math(display,display?"$$":"$");}
void Engine::mathEnvironment(const std::string& name,bool numbered){
    if(numbered){stepCounter("equation");setReferenceValue(std::to_string(counter("equation")));}
    std::vector<Token> body;std::vector<std::string> nested;bool ended=false;
    while(ensureToken()){
        auto t=pop();
        if(t.kind==TokenKind::Control&&!t.active&&t.value=="label") {label(readTextArgument());continue;}
        if(t.kind==TokenKind::Control&&!t.active&&(t.value=="begin"||t.value=="end")){
            auto arg=readArgument();auto env=tokensText(arg);
            if(t.value=="end"&&nested.empty()){
                if(env!=name)report("mismatched math environment end: "+env+" (expected "+name+")",true);
                ended=true;break;
            }
            if(t.value=="begin")nested.push_back(env);
            else if(nested.back()==env)nested.pop_back();
            else report("mismatched nested math environment end: "+env,true);
            body.push_back(t);body.push_back({TokenKind::BeginGroup,"{"});body.insert(body.end(),arg.begin(),arg.end());body.push_back({TokenKind::EndGroup,"}"});
        }else body.push_back(std::move(t));
    }
    if(!ended)report("unterminated math environment: "+name,true);
    startBlock(BlockKind::DisplayMath);addSpan(SpanKind::Math,tokensText(body));paragraph();endEnvironment(name);
}
void Engine::readInput(){
    std::string path=readTextArgument();
    if(path.empty()){report("missing input filename",true);return;}
    if(includeDepth_>=16){report("input nesting limit exceeded",true);return;}
    std::filesystem::path target=path;
    if(target.extension().empty())target+=".tex";
    if(target.is_relative()&&file_!="<memory>")target=file_.parent_path()/target;
    try {
        std::string bytes=readFile(target);
        auto previousFile=file_;auto previousToken=current_;bool previousEnd=endInput_;
        file_=target;++includeDepth_;endInput_=false;
        try {executeSource(std::move(bytes));}
        catch(...) {file_=previousFile;current_=previousToken;--includeDepth_;endInput_=previousEnd;throw;}
        file_=previousFile;current_=previousToken;--includeDepth_;endInput_=previousEnd;
    }catch(const std::exception& ex){report(ex.what(),true);}
}
void Engine::conditional(bool yes){
    if(conditions_.size()>=1024)throw std::runtime_error("conditional nesting limit exceeded");
    conditions_.push_back(false);
    if(!yes)finishConditional(true);
}
void Engine::finishConditional(bool alternative){
    if(conditions_.size()<=conditionalFloor_){report(alternative?"unexpected \\else":"unexpected \\fi",true);return;}
    if(!alternative){conditions_.pop_back();return;}
    bool takeElse=!conditions_.back();conditions_.back()=true;
    int nesting=0;
    while(ensureToken()){
        Token t=pop();
        if(t.kind!=TokenKind::Control||t.active)continue;
        if(t.value=="iftrue"||t.value=="iffalse"||t.value=="ifdefined"||t.value=="ifnum"||t.value=="ifdim")++nesting;
        else if(t.value=="fi"){
            if(!nesting){conditions_.pop_back();return;}--nesting;
        }else if(t.value=="else"&&!nesting&&takeElse)return;
    }
    conditions_.pop_back();report("unterminated conditional",true);
}
void Engine::command(const std::string& name){
    for(std::size_t i=scopes_.size();i-->0;){
        if(scopes_[i].contains(name))break;
        if(auto it=commandScopes_[i].find(name);it!=commandScopes_[i].end()){auto handler=it->second;handler(*this);return;}
    }
    if(const Macro* m=lookup(name)) {Macro copy=*m;expand(copy);return;}
    if(auto it=handlers_.find(name);it!=handlers_.end()){auto handler=it->second;handler(*this);return;}
    if(name==" "||name=="~") {addText(" ");return;}
    if(name=="\\"){addText("\n");return;}
    if(name.size()==1 && std::string("%$&#_{}^\\").find(name[0])!=std::string::npos) {addText(name);return;}
    if(name=="-"||name=="/"){addText(name=="-"?"\xC2\xAD":"");return;}
    if(name.empty())return;
    report("undefined control sequence: \\"+name,true);
}
void Engine::run(bool /*nested*/){
    auto conditionDepth=conditions_.size();
    while(!stopped_ && !endInput_ && ensureToken()) {
        Token t=expanded();
        if(t.kind==TokenKind::Sentinel)break;
        current_=t;
        if(t.kind==TokenKind::Control&&t.expansionProtected)continue;
        switch(t.kind){
            case TokenKind::Control: command(t.controlName());break;
            case TokenKind::BeginGroup:beginGroup();break;
            case TokenKind::EndGroup:endGroup();break;
            case TokenKind::MathShift:processMathShift();break;
            case TokenKind::Space:addSpace();break;
            case TokenKind::EndLine:
                if(++successiveNewlines_>=2)flush();else addSpace();
                break;
            case TokenKind::Alignment:addText(" & ");break;
            case TokenKind::Superscript:case TokenKind::Subscript:addText(t.value);break;
            case TokenKind::Parameter:report("parameter token outside macro definition",true);break;
            case TokenKind::Character:addText(t.value);break;
            case TokenKind::Sentinel:break;
        }
    }
    if(conditions_.size()>conditionDepth){report("unterminated conditional",true);conditions_.resize(conditionDepth);}
}
Document Engine::compile(std::string source,std::filesystem::path filename){
    return compileImpl(std::move(source),std::move(filename),true);
}
Document Engine::compileFragment(std::string source,std::filesystem::path filename){
    return compileImpl(std::move(source),std::move(filename),false);
}
Document Engine::compileImpl(std::string source,std::filesystem::path filename,bool reset){
    // Document-defined environments/commands must not leak into another compile.
    auto handlers=handlers_;auto environments=environments_;auto expandablePrimitives=expandablePrimitives_;auto registerPrimitives=registerPrimitives_;
    if(reset){document_={};scopes_.front().clear();commandScopes_.front().clear();environmentScopes_.front().clear();registerScopes_.front().clear();registerAliasScopes_.front().clear();counters_.clear();lastHeadingNumber_.clear();lastHeadingId_.clear();headingSequence_=0;}
    else {document_.blocks.clear();document_.diagnostics.clear();}
    file_=std::move(filename);scopes_.resize(1);commandScopes_.resize(1);
    environmentScopes_.resize(1);groupFloor_=1;
    if(reset)counterParents_.clear();
    registerScopes_.resize(1);registerAliasScopes_.resize(1);globalAssignment_=false;
    // Discard any incomplete local groups left by a previous fragment/error.
    while(!catcodeSaves_.empty()){
        for(auto [character,code]:catcodeSaves_.back())catcodes_[character]=code;
        catcodeSaves_.pop_back();
    }
    if(reset)catcodes_=Lexer::defaultCatcodes();
    conditions_.clear();conditionalFloor_=0;source_.reset();afterGroups_.clear();
    stream_.clear();envStack_.clear();styles_.clear();groupStyles_.clear();active_={};activeValid_=false;
    envEnds_.clear();envEndTokens_.clear();envGroupDepths_.clear();envContextDepths_.clear();envItems_.clear();blockContexts_.clear();
    pendingSpace_=false;successiveNewlines_=0;operations_=0;
    includeDepth_=executionDepth_=expansionDepth_=0;stopped_=endInput_=false;current_={};
    try {source_.emplace(std::move(source));run(false);flush();}
    catch(...) {handlers_=std::move(handlers);environments_=std::move(environments);expandablePrimitives_=std::move(expandablePrimitives);registerPrimitives_=std::move(registerPrimitives);scopes_.resize(1);commandScopes_.resize(1);registerScopes_.resize(1);registerAliasScopes_.resize(1);throw;}
    if(reset){handlers_=std::move(handlers);environments_=std::move(environments);expandablePrimitives_=std::move(expandablePrimitives);registerPrimitives_=std::move(registerPrimitives);}
    if(!envStack_.empty())report("unclosed environment: "+envStack_.back(),true);
    if(scopes_.size()!=1)report("unclosed group",true);
    return document_;
}
void Engine::setCounter(const std::string& n,int v){counters_[n]=v;}
bool Engine::hasCounter(const std::string& n)const{
    return counters_.contains(n)||n=="section"||n=="subsection"||n=="subsubsection"||n=="equation"||n=="figure"||n=="table"||n=="footnote";
}
void Engine::defineCounter(const std::string& n,const std::string& parent){
    if(n.empty()||hasCounter(n)){report("counter already defined or empty: "+n,true);return;}
    if(!parent.empty()&&!hasCounter(parent)){report("unknown parent counter: "+parent,true);return;}
    counters_[n]=0;if(!parent.empty())counterParents_[n]=parent;
}
std::string Engine::formattedCounter(const std::string& n)const{
    std::vector<int> parts;auto name=n;
    for(;;){parts.push_back(counter(name));auto found=counterParents_.find(name);if(found==counterParents_.end())break;name=found->second;}
    std::string result;for(auto i=parts.rbegin();i!=parts.rend();++i){if(!result.empty())result+='.';result+=std::to_string(*i);}return result;
}
int Engine::counter(const std::string& n)const{auto it=counters_.find(n);return it==counters_.end()?0:it->second;}
void Engine::stepCounter(const std::string& n){
    int before=counter(n);addToCounter(n,1);if(counter(n)==before)return;
    std::vector<std::string> parents{n};
    for(std::size_t i=0;i<parents.size();++i)
        for(const auto& [child,parent]:counterParents_)if(parent==parents[i]){counters_[child]=0;parents.push_back(child);}
}
void Engine::addToCounter(const std::string& n,int delta){
    int value=counter(n);
    if((delta>0 && value>std::numeric_limits<int>::max()-delta) ||
       (delta<0 && value<std::numeric_limits<int>::min()-delta)){
        report("counter overflow: "+n,true);return;
    }
    counters_[n]=value+delta;
}
void Engine::label(std::string key){document_.references[std::move(key)]=lastHeadingNumber_.empty()?"1":lastHeadingNumber_;}
void Engine::reference(std::string key){addSpan(SpanKind::Link,key,"ref:"+key);}
void Engine::setReferenceValue(std::string value){lastHeadingNumber_=std::move(value);}
std::filesystem::path Engine::currentFile()const{return file_;}
}
