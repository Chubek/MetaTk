#include "VM.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace metavm {
namespace {
std::int64_t signed_bits(std::uint64_t n) { std::int64_t v; std::memcpy(&v, &n, 8); return v; }
bool name_ok(std::string_view s) {
    auto alpha=[](char c) { return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_'; };
    if (s.empty() || s.size()>255 || !alpha(s[0])) return false;
    return std::all_of(s.begin(),s.end(),[&](char c) { return alpha(c) || (c>='0' && c<='9') || c=='.'; });
}
void put(std::vector<std::uint8_t> &v, std::uint64_t n, unsigned width) {
    for (unsigned i=0;i<width;++i) v.push_back(static_cast<std::uint8_t>(n>>(8*i)));
}
}
Program::Program(std::uint32_t slots, std::uint32_t parameters, std::vector<Instruction> code,
                 std::vector<std::string> hosts, Limits limits)
    : slots_(slots), parameters_(parameters), code_(std::move(code)), hosts_(std::move(hosts)) {
    if (!slots || slots>limits.slots || parameters>slots || code_.empty() ||
        code_.size()>limits.instructions || hosts_.size()>limits.hosts)
        throw std::invalid_argument("MetaVM: invalid program size or parameter count");
    std::set<std::string> names;
    for (const auto &name:hosts_) if (!name_ok(name) || !names.insert(name).second)
        throw std::invalid_argument("MetaVM: invalid or duplicate host name");
    auto reg=[&](std::uint32_t n) { if (n>=slots) throw std::invalid_argument("MetaVM: register outside frame"); };
    auto target=[&](std::uint64_t n) { if (n>=code_.size()) throw std::invalid_argument("MetaVM: branch outside program"); };
    for (std::size_t pc=0;pc<code_.size();++pc) {
        const auto &i=code_[pc];
        if (i.op>Op::Return) throw std::invalid_argument("MetaVM: unknown opcode");
        if (i.op<=Op::Lor) {
            reg(i.dst);
            if (i.op!=Op::Constant) reg(i.a);
            if (i.op>=Op::Add && i.op<=Op::Lor && !(i.op>=Op::Neg && i.op<=Op::Lnot)) reg(i.b);
        } else if (i.op==Op::Jump) target(i.a);
        else if (i.op==Op::Branch) { reg(i.a); target(i.b); target(static_cast<std::uint64_t>(i.imm)); }
        else if (i.op==Op::Return) reg(i.a);
        else {
            reg(i.dst);
            if (i.a>=hosts_.size() || i.imm<0 || i.imm>64 || i.b>slots ||
                static_cast<std::uint64_t>(i.imm)>slots-i.b)
                throw std::invalid_argument("MetaVM: invalid host call");
        }
        if (pc+1==code_.size() && i.op!=Op::Jump && i.op!=Op::Branch && i.op!=Op::Return)
            throw std::invalid_argument("MetaVM: instruction falls off program");
    }
}
std::vector<std::uint8_t> Program::serialize() const {
    std::vector<std::uint8_t> out{'M','V','M',0};
    put(out,version,4); put(out,slots_,4); put(out,parameters_,4);
    put(out,code_.size(),4); put(out,hosts_.size(),4);
    for (const auto &name:hosts_) {
        put(out,name.size(),4); out.insert(out.end(),name.begin(),name.end());
    }
    for (const auto &i:code_) {
        put(out,static_cast<std::uint32_t>(i.op),4); put(out,i.dst,4); put(out,i.a,4); put(out,i.b,4);
        put(out,static_cast<std::uint64_t>(i.imm),8);
    }
    return out;
}
Program Program::deserialize(const std::vector<std::uint8_t> &bytes, Limits limits) {
    if (bytes.size()>limits.input_bytes) throw std::length_error("MetaVM: input byte limit exceeded");
    std::size_t pos=0;
    auto get=[&](unsigned n) {
        if (n>bytes.size()-pos) throw std::invalid_argument("MetaVM: truncated bytecode");
        std::uint64_t v=0;for (unsigned j=0;j<n;++j) v|=std::uint64_t(bytes[pos++])<<(8*j);return v;
    };
    if (get(4)!=0x004d564d) throw std::invalid_argument("MetaVM: bad bytecode magic");
    if (get(4)!=version) throw std::invalid_argument("MetaVM: unsupported bytecode version");
    auto slots=static_cast<std::uint32_t>(get(4)), params=static_cast<std::uint32_t>(get(4));
    auto count=get(4), host_count=get(4);
    if (!slots || slots>limits.slots || params>slots || !count || count>limits.instructions || host_count>limits.hosts)
        throw std::invalid_argument("MetaVM: bytecode size limit exceeded");
    std::vector<std::string> hosts;
    for (std::uint64_t j=0;j<host_count;++j) {
        auto n=get(4);
        if (n>255 || n>bytes.size()-pos) throw std::invalid_argument("MetaVM: invalid host name length");
        hosts.emplace_back(reinterpret_cast<const char *>(bytes.data()+pos),static_cast<std::size_t>(n));pos+=n;
    }
    if (count>(bytes.size()-pos)/24 || count*24!=bytes.size()-pos)
        throw std::invalid_argument("MetaVM: truncated instructions or trailing bytecode");
    std::vector<Instruction> code;code.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t j=0;j<count;++j) {
        auto op=static_cast<Op>(get(4));auto dst=static_cast<std::uint32_t>(get(4));
        auto a=static_cast<std::uint32_t>(get(4)), b=static_cast<std::uint32_t>(get(4));
        code.push_back({op,dst,a,b,signed_bits(get(8))});
    }
    return Program(slots,params,std::move(code),std::move(hosts),limits);
}
Frame make_frame(const Program &p, const std::vector<std::int64_t> &args) {
    if (args.size()!=p.parameters()) throw std::invalid_argument("MetaVM: argument count mismatch");
    Frame f;f.program_identity=p.identity_;f.values.resize(p.slots());std::copy(args.begin(),args.end(),f.values.begin());return f;
}
void validate_frame(const Program &p, const Frame &f) {
    if (!p.identity_ || f.program_identity!=p.identity_ || f.active ||
        f.values.size()!=p.slots() || f.pc>=p.code().size() || f.status>Status::Trapped || f.trap>Trap::HostFailure ||
        ((f.status==Status::Trapped)!=(f.trap!=Trap::None)))
        throw std::invalid_argument("MetaVM: invalid frame state");
}
Stop run(const Program &p, Frame &f, std::uint64_t budget, const Host &host) {
    validate_frame(p,f);
    if (f.status!=Status::Running) return f.status==Status::Returned ? Stop::Returned : Stop::Trapped;
    if (budget>std::numeric_limits<std::uint64_t>::max()-f.steps)
        throw std::length_error("MetaVM: step counter overflow");
    f.active=true;struct Reset { bool &active;~Reset() { active=false; } } reset{f.active};
    auto trap=[&](Trap t) { f.trap=t;f.status=Status::Trapped; };
    while (budget--) {
        const auto &i=p.code()[static_cast<std::size_t>(f.pc)];
        ++f.steps;
        auto &v=f.values;
        // Read only operands used by this opcode; target/host indices are not registers.
        std::int64_t a=0,b=0,r=0;
        if (i.op<=Op::Lor && i.op!=Op::Constant) a=v[i.a];
        if (i.op>=Op::Add && i.op<=Op::Lor && !(i.op>=Op::Neg && i.op<=Op::Lnot)) b=v[i.b];
        auto ua=static_cast<std::uint64_t>(a), ub=static_cast<std::uint64_t>(b);
        switch (i.op) {
        case Op::Constant:r=i.imm;break;
        case Op::Move:r=a;break;
        case Op::Add:r=signed_bits(ua+ub);break;
        case Op::Sub:r=signed_bits(ua-ub);break;
        case Op::Mul:r=signed_bits(ua*ub);break;
        case Op::Div:case Op::Mod:
            if (!b) trap(Trap::DivisionByZero);
            else if (a==INT64_MIN && b==-1) trap(Trap::DivisionOverflow);
            else r=i.op==Op::Div ? a/b : a%b;
            break;
        case Op::Shl:case Op::Shr:
            if (b<0 || b>63) trap(Trap::InvalidShift);
            else if (i.op==Op::Shl) r=signed_bits(ua<<static_cast<unsigned>(b));
            else r=signed_bits((ua>>static_cast<unsigned>(b)) | (a<0 && b ? (~UINT64_C(0)<<(64-static_cast<unsigned>(b))) : 0));
            break;
        case Op::Band:r=signed_bits(ua&ub);break;
        case Op::Bor:r=signed_bits(ua|ub);break;
        case Op::Bxor:r=signed_bits(ua^ub);break;
        case Op::Neg:r=signed_bits(0-ua);break;
        case Op::Bnot:r=signed_bits(~ua);break;
        case Op::Lnot:r=!a;break;
        case Op::Eq:r=a==b;break;case Op::Ne:r=a!=b;break;
        case Op::Lt:r=a<b;break;case Op::Le:r=a<=b;break;
        case Op::Gt:r=a>b;break;case Op::Ge:r=a>=b;break;
        case Op::Land:r=a!=0 && b!=0;break;case Op::Lor:r=a!=0 || b!=0;break;
        case Op::Jump:f.pc=i.a;continue;
        case Op::Branch:f.pc=v[i.a]!=0 ? i.b : static_cast<std::uint64_t>(i.imm);continue;
        case Op::Return:f.result=v[i.a];f.status=Status::Returned;return Stop::Returned;
        case Op::Call: {
            if (!host) { trap(Trap::MissingHost);break; }
            std::vector<std::int64_t> args(v.begin()+i.b,v.begin()+i.b+static_cast<std::size_t>(i.imm));
            // Mark before invoking the host: an exception must never invite replay.
            trap(Trap::HostFailure);
            try { r=host(p.hosts()[i.a],args);f.status=Status::Running;f.trap=Trap::None; }
            catch (const std::bad_alloc &) { throw; }
            catch (const std::exception &e) { f.diagnostic=e.what(); }
            catch (...) { f.diagnostic="host threw a non-standard exception"; }
            break;
        }
        }
        if (f.status==Status::Trapped) return Stop::Trapped;
        v[i.dst]=r;++f.pc;
    }
    return Stop::Budget;
}
} // namespace metavm
