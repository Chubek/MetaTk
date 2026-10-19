#include "MetaAS.hpp"
#include "Expression.hpp"
#include "MetaArch.hpp"
#include <algorithm>
#include <cstring>
#include <set>

namespace metaas {
struct Assembler::Impl {
    metaarch::Spec spec;
    explicit Impl(metaarch::Spec s):spec(std::move(s)) {}
};
namespace {
using namespace detail;
using Arguments=std::vector<std::pair<std::string,std::string>>;

std::string_view uncomment(std::string_view text) {
    bool quoted=false,escape=false;
    for (std::size_t n=0;n<text.size();++n) {
        char c=text[n];
        if (quoted && escape) { escape=false; continue; }
        if (quoted && c=='\\') { escape=true; continue; }
        if (c=='"') quoted=!quoted;
        if (!quoted && (c=='#' || c==';')) return text.substr(0,n);
    }
    if (quoted) throw std::invalid_argument("unterminated string");
    return text;
}
std::vector<std::string> split(std::string_view text) {
    text=trim(text);
    if (text.empty()) return {};
    std::vector<std::string> parts;
    std::size_t begin=0;
    std::size_t depth=0; bool quote=false,escape=false;
    for (std::size_t n=0;n<=text.size();++n) {
        const char c=n<text.size()?text[n]:',';
        if (quote) {
            if (escape) escape=false;
            else if (c=='\\') escape=true;
            else if (c=='"') quote=false;
        } else if (c=='"') quote=true;
        else if (c=='(') ++depth;
        else if (c==')') { if (!depth) throw std::invalid_argument("unmatched ')'"); --depth; }
        else if (c==',' && depth==0) {
            auto part=trim(text.substr(begin,n-begin));
            if (part.empty()) throw std::invalid_argument("empty operand");
            parts.emplace_back(part); begin=n+1;
        }
    }
    if (quote || depth) throw std::invalid_argument("unterminated operand");
    return parts;
}
std::vector<std::uint8_t> string_bytes(std::string_view text) {
    if (text.size()<2 || text.front()!='"' || text.back()!='"') throw std::invalid_argument("expected double-quoted string");
    std::vector<std::uint8_t> bytes;
    auto digit=[](char c)->int {
        return c>='0'&&c<='9' ? c-'0' : c>='a'&&c<='f' ? c-'a'+10 : c>='A'&&c<='F' ? c-'A'+10 : -1;
    };
    for (std::size_t n=1;n+1<text.size();++n) {
        unsigned char c=static_cast<unsigned char>(text[n]);
        if (c=='"') throw std::invalid_argument("unescaped quote in string");
        if (c=='\\') {
            if (++n+1>=text.size()) throw std::invalid_argument("incomplete string escape");
            switch (text[n]) {
            case 'n': c='\n'; break; case 'r': c='\r'; break; case 't': c='\t'; break;
            case '0': c=0; break; case '\\': c='\\'; break; case '"': c='"'; break;
            case 'x':
                if (n+3>=text.size() || digit(text[n+1])<0 || digit(text[n+2])<0) throw std::invalid_argument("expected two hex digits after \\x");
                c=static_cast<unsigned char>(digit(text[n+1])*16+digit(text[n+2])); n+=2; break;
            default: throw std::invalid_argument("unknown string escape");
            }
        }
        bytes.push_back(c);
    }
    return bytes;
}
bool equal_name(std::string_view a,std::string_view b,bool ci) {
    if (a.size()!=b.size()) return false;
    auto lower=[](char c) { return c>='A'&&c<='Z' ? static_cast<char>(c-'A'+'a') : c; };
    for (std::size_t n=0;n<a.size();++n) if ((ci?lower(a[n]):a[n])!=(ci?lower(b[n]):b[n])) return false;
    return true;
}
struct Statement {
    enum Kind { Instruction,Data,Bytes,Fill } kind=Instruction;
    std::size_t section=0,offset=0,size=0,line=0,column=0,width=0,encoder=0;
    Arguments operands;
    std::vector<std::uint8_t> bytes;
    std::uint8_t fill=0;
    bool raw=false;
};
class Assembly {
    const metaarch::Spec& owner_;
    const DsymSpec& spec_;
    const Options& options_;
    std::string file_;
    Image image_;
    Bindings bindings_;
    std::vector<std::size_t> sizes_;
    std::vector<Statement> statements_;
    std::map<std::string,Symbol,std::less<>> symbols_;
    std::map<std::string,std::int64_t,std::less<>> constants_;
    std::set<std::string,std::less<>> externals_;
    std::size_t current_=0,total_=0,items_=0;
    bool big_=false;
public:
    std::size_t line=0,column=0;
private:
    void name(std::string_view value) const {
        if (!identifier(value)) throw std::invalid_argument("invalid identifier: " + std::string(value));
    }
    void symbol_name(const std::string& value) const {
        name(value);
        if (constants_.contains(value) || owner_.find_register(value)) throw std::invalid_argument("symbol conflicts with constant or register: " + value);
    }
    Symbol& symbol(const std::string& value) {
        symbol_name(value);
        auto it=symbols_.find(value);
        if (it!=symbols_.end()) return it->second;
        if (symbols_.size()+constants_.size()>=options_.max_symbols) throw std::invalid_argument("symbol limit exceeded");
        return symbols_.emplace(value,Symbol{value,std::nullopt,0,false}).first->second;
    }
    void section(const std::string& value) {
        name(value);
        for (std::size_t n=0;n<image_.sections.size();++n) if (image_.sections[n].name==value) { current_=n; return; }
        if (image_.sections.size()>=options_.max_sections) throw std::invalid_argument("section limit exceeded");
        current_=image_.sections.size();
        image_.sections.push_back({value,0,1,{}}); sizes_.push_back(0);
    }
    void append(Statement s) {
        if (s.size>options_.max_output_bytes-total_) throw std::invalid_argument("output byte limit exceeded");
        s.section=current_; s.offset=sizes_[current_]; s.line=line; s.column=column;
        total_+=s.size; sizes_[current_]+=s.size;
        statements_.push_back(std::move(s));
    }
    Value evaluate(std::string_view text,std::size_t section_index,std::size_t offset,bool numeric,bool absolute=false) const {
        auto resolve=[&](std::string_view token)->Value {
            if (auto c=constants_.find(token); c!=constants_.end()) return {c->second,{}};
            if (absolute) throw std::invalid_argument("layout expressions require previously defined absolute constants: " + std::string(token));
            if (token==".") return {static_cast<std::int64_t>(image_.sections[section_index].address+offset),{}};
            auto found=symbols_.find(token);
            if (found==symbols_.end()) throw std::invalid_argument("undefined symbol: " + std::string(token));
            const auto& sym=found->second;
            if (numeric) {
                if (!sym.section) throw std::invalid_argument("external symbol needs a relocation: " + sym.name);
                return {static_cast<std::int64_t>(image_.sections[*sym.section].address+sym.offset),{}};
            }
            return {0,sym.name};
        };
        auto difference=[&](const Value& a,const Value& b) {
            const auto& left=symbols_.at(a.symbol); const auto& right=symbols_.at(b.symbol);
            if (!left.section || left.section!=right.section) throw std::invalid_argument("symbol difference requires two defined symbols in the same section");
            return add(sub(static_cast<std::int64_t>(left.offset),static_cast<std::int64_t>(right.offset)),sub(a.number,b.number));
        };
        return Expression(text,std::min<std::size_t>(options_.max_expression_depth,256),resolve,difference).run();
    }
    std::int64_t absolute(std::string_view text) const { return evaluate(text,current_,sizes_[current_],true,true).number; }
    std::size_t encoder(std::string_view op,bool raw) const {
        auto named=dsym_spec_find_encoder(&spec_,std::string(op).c_str());
        if (named>=0) return static_cast<std::size_t>(named);
        std::set<std::size_t> found;
        if (!raw) for (std::size_t n=0;n<spec_.mnemonic_count;++n) {
            const auto& m=spec_.mnemonics[n]; bool matches=equal_name(op,m.name,m.ci);
            for (std::size_t a=0;a<m.aliases.count;++a) matches=matches || equal_name(op,m.aliases.items[a],m.ci);
            if (matches) for (std::size_t f=0;f<spec_.form_count;++f)
                if (spec_.forms[f].mnemonic_index==static_cast<std::int32_t>(n) && spec_.forms[f].encoder_index>=0)
                    found.insert(static_cast<std::size_t>(spec_.forms[f].encoder_index));
        }
        if (found.empty()) throw std::invalid_argument("unknown instruction/encoder: " + std::string(op));
        if (found.size()!=1) throw std::invalid_argument("ambiguous mnemonic; select an explicit encoder with .insn: " + std::string(op));
        return *found.begin();
    }
    void validate_bindings() const {
        for (const auto& [enc,params]:bindings_) {
            auto index=dsym_spec_find_encoder(&spec_,enc.c_str());
            if (index<0) throw std::invalid_argument("binding names unknown encoder: " + enc);
            auto p=spec_.encoders[index].program;
            if (!p || params.size()!=p->param_count) throw std::invalid_argument("binding must cover every encoder parameter: " + enc);
            std::set<std::string> seen;
            for (const auto& param:params)
                if (!seen.insert(param).second || dsym_program_param_index(p,param.c_str())<0)
                    throw std::invalid_argument("duplicate or unknown binding parameter: " + param);
        }
    }
    void instruction(std::string_view op,std::string_view args,bool raw) {
        Statement s; s.encoder=encoder(op,raw); s.raw=raw;
        const auto& enc=spec_.encoders[s.encoder]; auto p=enc.program;
        if (!p) throw std::invalid_argument("encoder is not compiled");
        s.size=p->static_size;
        auto parts=split(args); bool named=!parts.empty() && parts.front().find('=')!=std::string::npos;
        auto binding=bindings_.find(enc.name);
        if (!named && !parts.empty() && binding==bindings_.end()) throw std::invalid_argument("positional operands require --bind for encoder " + std::string(enc.name));
        if (parts.size()!=p->param_count) throw std::invalid_argument("wrong operand count for " + std::string(enc.name));
        std::set<std::string> seen;
        for (std::size_t n=0;n<parts.size();++n) {
            auto eq=parts[n].find('=');
            if ((eq!=std::string::npos)!=named) throw std::invalid_argument("cannot mix named and positional operands");
            auto key=named ? std::string(trim(std::string_view(parts[n]).substr(0,eq))) : binding->second[n];
            auto value=named ? std::string(trim(std::string_view(parts[n]).substr(eq+1))) : parts[n];
            if (value.empty() || !seen.insert(key).second || dsym_program_param_index(p,key.c_str())<0)
                throw std::invalid_argument("duplicate, missing or unknown operand: " + key);
            s.operands.emplace_back(std::move(key),std::move(value));
        }
        append(std::move(s));
    }
    void directive(std::string_view op,std::string_view tail) {
        if (op==".text" || op==".data" || op==".rodata" || op==".bss") {
            if (!trim(tail).empty()) throw std::invalid_argument("section shorthand takes no arguments");
            section(std::string(op)); return;
        }
        auto args=split(tail);
        if (op==".section") {
            if (args.size()!=1) throw std::invalid_argument(".section requires one name");
            section(args[0]); return;
        }
        if (op==".global" || op==".globl" || op==".extern") {
            if (args.empty()) throw std::invalid_argument("symbol directive requires names");
            for (const auto& a:args) { auto& sym=symbol(a); sym.global=true; if (op==".extern") externals_.insert(a); }
            return;
        }
        if (op==".equ") {
            if (args.size()!=2) throw std::invalid_argument(".equ requires name, expression");
            name(args[0]);
            if (symbols_.contains(args[0]) || constants_.contains(args[0]) || owner_.find_register(args[0])) throw std::invalid_argument("duplicate/conflicting constant: " + args[0]);
            if (symbols_.size()+constants_.size()>=options_.max_symbols) throw std::invalid_argument("symbol limit exceeded");
            auto value=absolute(args[1]); constants_.emplace(args[0],value); return;
        }
        Statement s;
        if (op==".byte" || op==".short" || op==".word" || op==".quad") {
            if (args.empty()) throw std::invalid_argument("data directive requires values");
            s.kind=Statement::Data; s.width=op==".byte"?1:op==".short"?2:op==".word"?4:8;
            if (args.size()>options_.max_output_bytes/s.width) throw std::invalid_argument("output byte limit exceeded");
            s.size=args.size()*s.width;
            for (auto& a:args) s.operands.emplace_back("",std::move(a));
        } else if (op==".ascii" || op==".asciz") {
            if (args.empty()) throw std::invalid_argument("string directive requires strings");
            s.kind=Statement::Bytes;
            for (const auto& a:args) {
                auto bytes=string_bytes(a);
                if (op==".asciz") bytes.push_back(0);
                if (bytes.size()>options_.max_output_bytes-s.bytes.size()) throw std::invalid_argument("output byte limit exceeded");
                s.bytes.insert(s.bytes.end(),bytes.begin(),bytes.end());
            }
            s.size=s.bytes.size();
        } else if (op==".zero" || op==".space" || op==".align" || op==".org") {
            if (args.empty() || args.size()>2 || (op==".zero" && args.size()!=1)) throw std::invalid_argument("expected size/alignment[, fill]");
            auto count=absolute(args[0]);
            if (count<0 || static_cast<std::uint64_t>(count)>options_.max_output_bytes) throw std::invalid_argument("size/alignment exceeds output limit");
            auto fill=args.size()==2 ? absolute(args[1]) : 0;
            if (fill<0 || fill>255) throw std::invalid_argument("fill must be in 0..255");
            s.kind=Statement::Fill; s.fill=static_cast<std::uint8_t>(fill); s.size=static_cast<std::size_t>(count);
            if (op==".align") {
                auto next=aligned(sizes_[current_],static_cast<std::uint64_t>(count));
                s.size=static_cast<std::size_t>(next-sizes_[current_]);
                image_.sections[current_].alignment=std::max(image_.sections[current_].alignment,static_cast<std::size_t>(count));
            } else if (op==".org") {
                if (s.size<sizes_[current_]) throw std::invalid_argument(".org cannot move backwards");
                s.size-=sizes_[current_];
            }
        } else throw std::invalid_argument("unknown directive: " + std::string(op));
        append(std::move(s));
    }
    void parse_line(std::string_view text) {
        auto original=text;
        text=trim(uncomment(text));
        while (!text.empty()) {
            if (++items_>options_.max_statements) throw std::invalid_argument("statement limit exceeded");
            column=static_cast<std::size_t>(text.data()-original.data())+1;
            std::size_t n=0;
            while (n<text.size() && rest(text[n])) ++n;
            if (n<text.size() && text[n]==':') {
                auto& sym=symbol(std::string(text.substr(0,n)));
                if (sym.section) throw std::invalid_argument("duplicate label: " + sym.name);
                sym.section=current_; sym.offset=sizes_[current_];
                text=trim(text.substr(n+1)); continue;
            }
            auto end=text.find_first_of(" \t");
            auto op=text.substr(0,end);
            auto tail=end==text.npos ? std::string_view{} : trim(text.substr(end));
            if (op==".insn") {
                end=tail.find_first_of(" \t"); op=tail.substr(0,end);
                auto operands=end==tail.npos ? std::string_view{} : trim(tail.substr(end));
                instruction(op,operands,true);
            } else if (op.starts_with('.')) directive(op,tail);
            else instruction(op,tail,false);
            break;
        }
    }
    bool check_form(const DsymForm& form,const Statement& s,const std::map<std::string,std::int64_t>& values,
                    const std::map<std::string,const DsymRegister*>& regs,
                    const std::map<std::string,Value>& symbolic,std::string& error) const {
        auto reject=[&](std::string why) { error=std::move(why); return false; };
        const auto& features=spec_.targets[options_.target].features;
        auto enabled=[&](std::string_view feature) {
            for (std::size_t n=0;n<features.count;++n) if (feature==features.items[n]) return true;
            return false;
        };
        for (std::size_t n=0;n<form.required.count;++n) if (!enabled(form.required.items[n])) return reject("required target feature: " + std::string(form.required.items[n]));
        for (std::size_t n=0;n<form.forbidden.count;++n) if (enabled(form.forbidden.items[n])) return reject("forbidden target feature: " + std::string(form.forbidden.items[n]));
        if (!form.has_operands) return reject("form has no operand metadata");
        const std::vector<std::string> empty_order;
        const auto binding=bindings_.find(spec_.encoders[s.encoder].name);
        const auto& order=binding==bindings_.end() ? empty_order : binding->second;
        std::size_t visible=0;
        for (std::size_t n=0;n<form.operand_count;++n) {
            const auto& operand=form.operands[n];
            if (operand.hidden || operand.kind==DSYM_OK_IMPLICIT) continue;
            if (visible>=order.size()) return reject("form operand count mismatch");
            const auto& param=order[visible++];
            auto fail=[&](const char *why) { return reject("operand " + std::to_string(n+1) + " ("+param+"): "+why); };
            if (operand.constraint) return fail("custom constraints require .insn low-level mode");
            if (operand.kind==DSYM_OK_REG) {
                auto r=regs.find(param);
                if (r==regs.end()) return fail("expected register name");
                if (operand.has_reg_class && r->second->reg_class!=operand.reg_class) return fail("wrong register class");
                if (operand.has_width && r->second->width!=operand.width) return fail("wrong register width");
            } else {
                if (operand.kind==DSYM_OK_MEM || operand.kind==DSYM_OK_REGLIST) return fail("complex operands require an explicit low-level encoder");
                if (regs.contains(param)) return fail("expected integer or symbol");
                // An addend is not the final relocated operand. Its field range
                // can only be checked once the linker has resolved the symbol.
                if (symbolic.contains(param)) continue;
                auto v=values.at(param);
                if ((operand.has_imm_min && v<operand.imm_min) || (operand.has_imm_max && v>operand.imm_max)) return fail("integer outside form bounds");
                if (operand.has_width && operand.width>0 && operand.width<64) {
                    if (operand.has_signed && operand.is_signed) {
                        auto bound=INT64_C(1)<<(operand.width-1);
                        if (v< -bound || v>=bound) return fail("integer outside signed width");
                    } else if (v<0 || static_cast<std::uint64_t>(v)>=(UINT64_C(1)<<operand.width)) return fail("integer outside unsigned width");
                } else if (operand.has_signed && !operand.is_signed && v<0) return fail("negative unsigned operand");
            }
        }
        return visible==order.size() || reject("form operand count mismatch");
    }
    void emit_instruction(const Statement& s) {
        const auto& enc=spec_.encoders[s.encoder]; const auto& p=*enc.program;
        std::set<std::string> relocation_params;
        for (std::size_t n=0;n<p.stmt_count;++n) if (p.stmts[n].kind==DSYM_ST_RELOC) relocation_params.insert(p.params[p.stmts[n].sym_param]);
        std::map<std::string,std::int64_t> numbers;
        std::map<std::string,const DsymRegister*> regs;
        std::map<std::string,Value> symbolic;
        std::vector<const char*> names; std::vector<std::int64_t> values;
        for (const auto& [param,text]:s.operands) {
            Value value;
            if (auto reg=owner_.find_register(text)) { value.number=reg->enc; regs[param]=reg; }
            else value=evaluate(text,s.section,s.offset,!relocation_params.contains(param));
            numbers[param]=value.number;
            if (!value.symbol.empty()) symbolic[param]=value;
            names.push_back(param.c_str()); values.push_back(value.number);
        }
        if (!s.raw && (bindings_.contains(enc.name) || s.operands.empty())) {
            bool any=false,matched=false; std::string error;
            for (std::size_t n=0;n<spec_.form_count;++n) if (spec_.forms[n].encoder_index==static_cast<std::int32_t>(s.encoder)) {
                any=true;
                if (check_form(spec_.forms[n],s,numbers,regs,symbolic,error)) { matched=true; break; }
            }
            if (any && !matched) throw std::invalid_argument(error.empty()?"no matching instruction form":error);
        }
        DsymEncoded result{};
        if (!dsym_encode_named(&spec_,enc.name,options_.target,names.data(),values.data(),values.size(),false,&result))
            throw std::invalid_argument(result.error);
        if (result.size!=s.size) throw std::invalid_argument("encoder size changed after layout");
        auto& bytes=image_.sections[s.section].bytes;
        std::copy(result.bytes,result.bytes+result.size,bytes.begin()+static_cast<std::ptrdiff_t>(s.offset));
        for (std::size_t n=0;n<result.reloc_count;++n) {
            const auto& r=result.relocs[n];
            Relocation out; out.section=s.section; out.offset=s.offset+r.offset; out.size=r.size;
            out.type=r.reloc_name; out.parameter=p.params[r.sym_param]; out.addend=r.sym_value;
            if (auto sym=symbolic.find(out.parameter); sym!=symbolic.end()) { out.symbol=sym->second.symbol; out.addend=sym->second.number; }
            out.bit_offset=r.bit_offset; out.bit_width=r.bit_width; out.big_endian=r.endian==DSYM_ENDIAN_BIG;
            if (r.reloc_index>=0) {
                const auto& desc=spec_.relocs[r.reloc_index]; out.pc_relative=desc.pcrel; out.is_signed=desc.is_signed;
                if (desc.expr) out.expression=desc.expr;
                if (desc.has_addend) out.addend_mode=desc.addend==DSYM_ADD_EXPLICIT ? "explicit" : desc.addend==DSYM_ADD_INFIELD ? "infield" : "implied";
                if (desc.has_overflow) out.overflow=desc.overflow==DSYM_OVF_ERROR ? "error" : "truncate";
            }
            image_.relocations.push_back(std::move(out));
        }
    }
public:
    Assembly(const metaarch::Spec& owner,const Options& options,std::string file)
        :owner_(owner),spec_(*owner.get()),options_(options),file_(std::move(file)),bindings_(owner.assembly_bindings()) {}
    Image run(std::string_view source) {
        if (source.size()>options_.max_source_bytes) throw std::invalid_argument("source byte limit exceeded");
        if (source.find('\0')!=source.npos) throw std::invalid_argument("embedded NUL in assembly source");
        if (options_.target>=spec_.target_count) throw std::invalid_argument("target index out of range");
        if (options_.origin>INT64_MAX) throw std::invalid_argument("origin exceeds signed address range");
        const auto& target=spec_.targets[options_.target];
        image_.architecture=target.arch;
        if (target.subarch) image_.subarchitecture=target.subarch;
        if (target.obj_format) image_.object_format=target.obj_format;
        if (target.abi) image_.abi=target.abi;
        image_.origin=options_.origin;
        big_=target.byte_order==DSYM_BO_BIG || target.byte_order==DSYM_BO_BE;
        image_.target=options_.target; image_.address_width=target.addr_size; image_.big_endian=big_;
        for (const auto& [key,params]:options_.bindings) bindings_[key]=params;
        validate_bindings(); section(".text");
        line=1;
        while (!source.empty()) {
            auto end=source.find('\n'); parse_line(source.substr(0,end));
            if (end==source.npos) break;
            source.remove_prefix(end+1); ++line;
        }
        for (const auto& [key,sym]:symbols_) {
            if (!sym.section && !externals_.contains(key)) throw std::invalid_argument("global symbol is neither defined nor external: " + key);
            image_.symbols.push_back(sym);
        }
        auto address=options_.origin;
        for (std::size_t n=0;n<image_.sections.size();++n) {
            auto& sec=image_.sections[n]; address=aligned(address,sec.alignment);
            if (address>INT64_MAX || address-options_.origin>options_.max_output_bytes ||
                sizes_[n]>options_.max_output_bytes-(address-options_.origin) || sizes_[n]>INT64_MAX-address)
                throw std::invalid_argument("section layout exceeds output/address limit");
            sec.address=address; address+=sizes_[n]; sec.bytes.resize(sizes_[n]);
        }
        for (const auto& s:statements_) {
            line=s.line; column=s.column;
            auto& bytes=image_.sections[s.section].bytes;
            if (s.kind==Statement::Instruction) emit_instruction(s);
            else if (s.kind==Statement::Fill) std::fill_n(bytes.begin()+static_cast<std::ptrdiff_t>(s.offset),s.size,s.fill);
            else if (s.kind==Statement::Bytes) std::copy(s.bytes.begin(),s.bytes.end(),bytes.begin()+static_cast<std::ptrdiff_t>(s.offset));
            else for (std::size_t n=0;n<s.operands.size();++n) {
                const auto offset=s.offset+n*s.width;
                auto value=evaluate(s.operands[n].second,s.section,offset,false);
                if (value.symbol.empty()) store(bytes,offset,s.width,value.number,big_);
                else {
                    Relocation r; r.section=s.section; r.offset=offset; r.size=s.width; r.type="ABS"+std::to_string(8*s.width);
                    r.symbol=std::move(value.symbol); r.addend=value.number; r.bit_width=static_cast<std::uint32_t>(8*s.width);
                    r.big_endian=big_; r.data=true; image_.relocations.push_back(std::move(r));
                }
            }
        }
        return std::move(image_);
    }
};
} // namespace

Result<Assembler> Assembler::load(const std::string& path) {
    if (path.empty() || path.find('\0')!=path.npos) return {std::nullopt,{{path,"invalid architecture path",0,0}}};
    auto loaded=metaarch::Spec::load(path);
    if (!loaded) {
        Result<Assembler> result;
        for (const auto& d:loaded.error()->diagnostics) result.diagnostics.push_back({d.file,d.message,d.line,d.column});
        return result;
    }
    return {Assembler(std::make_shared<Impl>(std::move(*loaded.get()))),{}};
}
Result<Image> Assembler::assemble(std::string_view source,const Options& options,const std::string& file) const {
    if (!impl_) return {std::nullopt,{{file,"assembler is moved-from",0,0}}};
    Assembly assembly(impl_->spec,options,file);
    try { return {assembly.run(source),{}}; }
    catch (const std::exception& e) { return {std::nullopt,{{file,e.what(),assembly.line,assembly.column}}}; }
}
std::vector<Encoder> Assembler::encoders() const {
    std::vector<Encoder> result;
    if (!impl_) return result;
    const auto& s=*impl_->spec.get();
    for (std::size_t n=0;n<s.encoder_count;++n) {
        const auto& e=s.encoders[n]; Encoder out; out.name=e.name;
        if (e.program) {
            out.size=e.program->static_size;
            for (std::size_t p=0;p<e.program->param_count;++p) out.parameters.emplace_back(e.program->params[p]);
        }
        result.push_back(std::move(out));
    }
    return result;
}
std::size_t Assembler::target_count() const { return impl_ ? impl_->spec.get()->target_count : 0; }
} // namespace metaas
