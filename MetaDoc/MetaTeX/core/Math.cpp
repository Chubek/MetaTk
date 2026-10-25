#include "MetaTeX.h"
#include <cctype>
#include <map>
#include <string>
#include <stdexcept>
namespace metatex {
namespace {
std::string esc(std::string_view s){std::string o;for(char c:s){switch(c){case '&':o+="&amp;";break;case '<':o+="&lt;";break;case '>':o+="&gt;";break;case '"':o+="&quot;";break;default:o+=c;}}return o;}
class MathParser {
    std::string_view src_;std::size_t i_=0, depth_=0;
    struct Depth {
        std::size_t& value;
        explicit Depth(std::size_t& n):value(n){if(n>=128)throw std::runtime_error("math nesting limit exceeded");++n;}
        ~Depth(){--value;}
    };
    const std::map<std::string,std::string> symbols_={{"alpha","α"},{"beta","β"},{"gamma","γ"},{"delta","δ"},{"epsilon","ε"},{"varepsilon","ε"},{"theta","θ"},{"lambda","λ"},{"mu","μ"},{"nu","ν"},{"pi","π"},{"rho","ρ"},{"sigma","σ"},{"tau","τ"},{"phi","φ"},{"varphi","φ"},{"chi","χ"},{"psi","ψ"},{"omega","ω"},{"Gamma","Γ"},{"Delta","Δ"},{"Theta","Θ"},{"Lambda","Λ"},{"Pi","Π"},{"Sigma","Σ"},{"Phi","Φ"},{"Psi","Ψ"},{"Omega","Ω"},{"infty","∞"},{"sum","∑"},{"prod","∏"},{"int","∫"},{"oint","∮"},{"partial","∂"},{"nabla","∇"},{"times","×"},{"cdot","⋅"},{"pm","±"},{"mp","∓"},{"leq","≤"},{"geq","≥"},{"neq","≠"},{"approx","≈"},{"equiv","≡"},{"rightarrow","→"},{"to","→"},{"leftarrow","←"},{"Rightarrow","⇒"},{"Leftrightarrow","⇔"},{"forall","∀"},{"exists","∃"},{"in","∈"},{"notin","∉"},{"subset","⊂"},{"subseteq","⊆"},{"cup","∪"},{"cap","∩"},{"ldots","…"},{"cdots","⋯"},{"emptyset","∅"},{"degree","°"}};
    void space(){while(i_<src_.size() && std::isspace((unsigned char)src_[i_]))++i_;}
    std::string arg(){space();if(i_<src_.size() && src_[i_]=='{'){++i_;auto s=sequence('}');if(i_<src_.size())++i_;return s;}return atom();}
    std::string atom(){
        Depth guard(depth_);
        space();if(i_==src_.size())return "<mrow/>";
        char c=src_[i_++];
        if(c=='{'){auto s=sequence('}');if(i_<src_.size())++i_;return "<mrow>"+s+"</mrow>";}
        if(c=='\\'){
            std::string name;
            while(i_<src_.size() && std::isalpha((unsigned char)src_[i_]))name+=src_[i_++];
            if(name.empty()&&i_<src_.size())name+=src_[i_++];
            if(name=="frac"||name=="dfrac"||name=="tfrac"){
                auto a=arg(),b=arg();return "<mfrac>"+a+b+"</mfrac>";
            }
            if(name=="sqrt")return "<msqrt>"+arg()+"</msqrt>";
            if(name=="text"||name=="mathrm"||name=="operatorname"){
                space();if(i_<src_.size()&&src_[i_]=='{'){
                    ++i_;std::size_t start=i_;int depth=1;
                    while(i_<src_.size()&&depth){if(src_[i_]=='{')++depth;else if(src_[i_]=='}')--depth;if(depth)++i_;}
                    auto out="<mtext>"+esc(src_.substr(start,i_-start))+"</mtext>";
                    if(i_<src_.size())++i_;
                    return out;
                }
            }
            if(name=="left"||name=="right")return "";
            if(name==","||name==";"||name=="quad")return "<mspace width=\"0.3em\"/>";
            if(name=="overline")return "<mover>"+arg()+"<mo>¯</mo></mover>";
            auto it=symbols_.find(name);
            if(it!=symbols_.end())return "<mo>"+it->second+"</mo>";
            return "<mi>"+esc(name)+"</mi>";
        }
        if(std::isdigit((unsigned char)c)){
            std::string s(1,c);while(i_<src_.size()&&(std::isdigit((unsigned char)src_[i_])||src_[i_]=='.'))s+=src_[i_++];
            return "<mn>"+s+"</mn>";
        }
        if(std::isalpha((unsigned char)c) || (unsigned char)c>=128){
            std::string s(1,c);while(i_<src_.size()&&(unsigned char)src_[i_]>=128)s+=src_[i_++];
            return "<mi>"+esc(s)+"</mi>";
        }
        return "<mo>"+esc(std::string(1,c))+"</mo>";
    }
    std::string sequence(char until=0){
        Depth guard(depth_);
        std::string out;
        while(i_<src_.size() && (!until || src_[i_]!=until)){
            space();if(i_==src_.size() || (until&&src_[i_]==until))break;
            std::string base=atom();
            std::string sup,sub;
            for(int tries=0;tries<2&&i_<src_.size();++tries){
                space();if(i_==src_.size())break;
                if(src_[i_]=='^' && sup.empty()){++i_;sup=arg();}
                else if(src_[i_]=='_' && sub.empty()){++i_;sub=arg();}
                else break;
            }
            if(!sup.empty()&&!sub.empty())base="<msubsup>"+base+sub+sup+"</msubsup>";
            else if(!sup.empty())base="<msup>"+base+sup+"</msup>";
            else if(!sub.empty())base="<msub>"+base+sub+"</msub>";
            out+=base;
        }
        return out;
    }
public:
    explicit MathParser(std::string_view s):src_(s){}
    std::string parse(){return "<math xmlns=\"http://www.w3.org/1998/Math/MathML\"><mrow>"+sequence()+"</mrow></math>";}
};
}
std::string renderMathML(std::string_view tex){return MathParser(tex).parse();}
}
