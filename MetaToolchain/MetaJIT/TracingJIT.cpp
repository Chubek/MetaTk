#include "TracingJIT.hpp"
#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace metajit {
namespace {
using metavm::Op;
struct Recorded { std::uint64_t pc, next; };
class Emitter {
    std::vector<std::uint8_t> code_;
    struct Exit { std::size_t patch; NativeExit reason; std::uint64_t pc; metavm::Trap trap; };
    std::vector<Exit> exits_;
    void b(std::initializer_list<std::uint8_t> data) {
        if (data.size()>1024*1024-code_.size()) throw std::length_error("MetaJIT: trace exceeds 1 MiB");
        code_.insert(code_.end(),data.begin(),data.end());
    }
    void word(std::uint32_t n) { for (unsigned i=0;i<4;++i) b({static_cast<std::uint8_t>(n>>(8*i))}); }
    void wide(std::uint64_t n) { for (unsigned i=0;i<8;++i) b({static_cast<std::uint8_t>(n>>(8*i))}); }
    void field(std::uint8_t offset,std::uint32_t n) { b({0x48,0xc7,0x47,offset});word(n); }
    void pc(std::uint64_t n) { field(8,static_cast<std::uint32_t>(n)); }
    void load(std::uint32_t slot,bool right=false) { b({0x48,0x8b,static_cast<std::uint8_t>(right?0x8e:0x86)});word(slot*8); }
    void store(std::uint32_t slot) { b({0x48,0x89,0x86});word(slot*8); }
    void boolean(std::uint8_t cc) { b({0x0f,cc,0xc0,0x0f,0xb6,0xc0}); }
    std::size_t branch(std::uint8_t cc) { b({0x0f,cc});auto pos=code_.size();word(0);return pos; }
    void resolve(std::size_t patch) {
        auto distance=static_cast<std::int64_t>(code_.size())-static_cast<std::int64_t>(patch+4);
        if (distance<INT32_MIN || distance>INT32_MAX) throw std::length_error("MetaJIT: trace jump overflow");
        auto n=static_cast<std::uint32_t>(distance);
        for (unsigned i=0;i<4;++i) code_.at(patch+i)=static_cast<std::uint8_t>(n>>(8*i));
    }
    void exit(NativeExit reason) { b({0xb8});word(static_cast<std::uint32_t>(reason));b({0xc3}); }
    void conditional_exit(std::uint8_t cc,NativeExit why,std::uint64_t where,metavm::Trap trap=metavm::Trap::None) {
        exits_.push_back({branch(cc),why,where,trap});
    }
public:
    CodeBuffer compile(const metavm::Program &p,const std::vector<Recorded> &trace) {
#if defined(__linux__) && defined(__x86_64__)
        static_assert(std::is_standard_layout_v<NativeFrame> && sizeof(NativeFrame)==48);
        static_assert(offsetof(NativeFrame,values)==0 && offsetof(NativeFrame,pc)==8 &&
            offsetof(NativeFrame,remaining)==16 && offsetof(NativeFrame,status)==24 &&
            offsetof(NativeFrame,trap)==32 && offsetof(NativeFrame,result)==40);
#endif
        // Readable prefix for instrumented indirect calls, then ENDBR64.
        b({0,0,0,0,0,0,0,0,0xf3,0x0f,0x1e,0xfa,0x48,0x8b,0x37}); // rsi = frame->values
        for (const auto &step:trace) {
            const auto &i=p.code().at(static_cast<std::size_t>(step.pc));
            pc(step.pc);
            b({0x48,0x83,0x7f,0x10,0}); // compare remaining with zero
            conditional_exit(0x84,NativeExit::Budget,step.pc);
            b({0x48,0xff,0x4f,0x10}); // consume exactly one instruction
            if (i.op==Op::Jump) continue;
            if (i.op==Op::Branch) {
                load(i.a);b({0x48,0x85,0xc0});
                // Equal destinations need no speculative path guard.
                if (i.b!=static_cast<std::uint64_t>(i.imm)) {
                    bool yes=step.next==i.b;
                    conditional_exit(yes?0x84:0x85,NativeExit::Guard,
                        yes?static_cast<std::uint64_t>(i.imm):i.b);
                }
                continue;
            }
            if (i.op==Op::Return) {
                load(i.a);b({0x48,0x89,0x47,0x28});field(24,1);exit(NativeExit::Returned);continue;
            }
            if (i.op==Op::Call) throw std::logic_error("MetaJIT: host call inside native trace");
            if (i.op==Op::Constant) { b({0x48,0xb8});wide(static_cast<std::uint64_t>(i.imm)); }
            else {
                load(i.a);
                if (i.op>=Op::Add && i.op<=Op::Lor && !(i.op>=Op::Neg && i.op<=Op::Lnot)) load(i.b,true);
                switch (i.op) {
                case Op::Move:break;
                case Op::Add:b({0x48,0x01,0xc8});break;
                case Op::Sub:b({0x48,0x29,0xc8});break;
                case Op::Mul:b({0x48,0x0f,0xaf,0xc1});break;
                case Op::Band:b({0x48,0x21,0xc8});break;
                case Op::Bor:b({0x48,0x09,0xc8});break;
                case Op::Bxor:b({0x48,0x31,0xc8});break;
                case Op::Neg:b({0x48,0xf7,0xd8});break;
                case Op::Bnot:b({0x48,0xf7,0xd0});break;
                case Op::Lnot:b({0x48,0x85,0xc0});boolean(0x94);break;
                case Op::Div:case Op::Mod: {
                    b({0x48,0x85,0xc9});conditional_exit(0x84,NativeExit::Trapped,step.pc,metavm::Trap::DivisionByZero);
                    b({0x48,0x83,0xf9,0xff});auto normal=branch(0x85);
                    b({0x49,0xb8});wide(UINT64_C(1)<<63);
                    b({0x4c,0x39,0xc0});conditional_exit(0x84,NativeExit::Trapped,step.pc,metavm::Trap::DivisionOverflow);
                    resolve(normal);b({0x48,0x99,0x48,0xf7,0xf9});
                    if (i.op==Op::Mod) b({0x48,0x89,0xd0});
                    break;
                }
                case Op::Shl:case Op::Shr:
                    b({0x48,0x83,0xf9,0x3f});conditional_exit(0x87,NativeExit::Trapped,step.pc,metavm::Trap::InvalidShift);
                    b({0x48,0xd3,static_cast<std::uint8_t>(i.op==Op::Shl?0xe0:0xf8)});break;
                case Op::Land:case Op::Lor:
                    b({0x48,0x85,0xc0});boolean(0x95);
                    b({0x48,0x85,0xc9,0x0f,0x95,0xc1,0x0f,0xb6,0xc9,
                        static_cast<std::uint8_t>(i.op==Op::Land?0x21:0x09),0xc8});break;
                case Op::Eq:case Op::Ne:case Op::Lt:case Op::Le:case Op::Gt:case Op::Ge: {
                    constexpr std::uint8_t cc[]={0x94,0x95,0x9c,0x9e,0x9f,0x9d};
                    b({0x48,0x39,0xc8});boolean(cc[static_cast<unsigned>(i.op)-static_cast<unsigned>(Op::Eq)]);break;
                }
                default:throw std::logic_error("MetaJIT: invalid native trace operator");
                }
            }
            store(i.dst);
        }
        pc(trace.back().next);exit(NativeExit::Boundary);
        for (const auto &e:exits_) {
            resolve(e.patch);pc(e.pc);
            if (e.trap!=metavm::Trap::None) { field(24,2);field(32,static_cast<std::uint32_t>(e.trap)); }
            exit(e.reason);
        }
        CodeBuffer buffer;buffer.append(code_);buffer.entry("trace",8,1);return buffer;
    }
};
}
struct TracingJIT::Impl {
    metavm::Program program;
    TraceOptions options;
    TraceStats stats;
    bool busy=false;
    std::vector<bool> headers,disabled;
    std::vector<std::uint64_t> heat;
    struct Trace { Executable code;Transition enter; };
    std::map<std::uint64_t,Trace> traces;
    std::size_t code_bytes=0;
    Impl(metavm::Program p,TraceOptions opts):program(std::move(p)),options(opts),
        headers(program.code().size(),false),disabled(program.code().size(),false),heat(program.code().size(),0) {
        if (!options.hot_threshold || !options.max_trace_instructions || options.max_trace_instructions>4096 ||
            !options.max_traces || options.max_traces>4096 || !options.max_code_bytes || options.max_code_bytes>CodeBuffer::max_size)
            throw std::invalid_argument("MetaJIT: invalid trace limits");
        // The backend uses signed disp32 addressing and sign-extended imm32 PCs.
        // Programs outside that native envelope still have portable semantics.
        if (program.slots()>static_cast<std::uint32_t>(INT32_MAX/8) || program.code().size()>INT32_MAX)
            options.native=false;
        for (std::size_t pc=0;pc<program.code().size();++pc) {
            const auto &i=program.code()[pc];
            auto mark=[&](std::uint64_t to) { if (to<=pc) headers[static_cast<std::size_t>(to)]=true; };
            if (i.op==Op::Jump) mark(i.a);
            if (i.op==Op::Branch) { mark(i.b);mark(static_cast<std::uint64_t>(i.imm)); }
        }
    }
    void publish(const std::vector<Recorded> &record) {
        if (record.empty()) return;
        auto code=Emitter().compile(program,record).finalize();
        if (code.size()>options.max_code_bytes-code_bytes) { disabled[record.front().pc]=true;return; }
        auto entry=code.get<Transition>("trace");auto size=code.size();
        traces.emplace(record.front().pc,Trace{std::move(code),entry});code_bytes+=size;++stats.compiled;
    }
};
bool TracingJIT::native_supported() noexcept {
#if defined(__linux__) && defined(__x86_64__)
    return true;
#else
    return false;
#endif
}
TracingJIT::TracingJIT(metavm::Program p,TraceOptions options):impl_(std::make_unique<Impl>(std::move(p),options)) {}
TracingJIT::~TracingJIT()=default;
const metavm::Program &TracingJIT::program() const noexcept { return impl_->program; }
const TraceStats &TracingJIT::stats() const noexcept { return impl_->stats; }
metavm::Frame TracingJIT::execute(const std::vector<std::int64_t> &args,std::uint64_t budget,const metavm::Host &host) {
    auto frame=metavm::make_frame(impl_->program,args);run(frame,budget,host);return frame;
}
metavm::Stop TracingJIT::run(metavm::Frame &frame,std::uint64_t budget,const metavm::Host &host) {
    auto &s=*impl_;
    if (s.busy) throw std::logic_error("MetaJIT: tracing callback reentry");
    metavm::validate_frame(s.program,frame);
    if (frame.status!=metavm::Status::Running)
        return frame.status==metavm::Status::Returned ? metavm::Stop::Returned : metavm::Stop::Trapped;
    if (budget>std::numeric_limits<std::uint64_t>::max()-frame.steps) throw std::length_error("MetaJIT: step counter overflow");
    s.busy=true;struct Reset { bool &b;~Reset() { b=false; } } reset{s.busy};
    std::vector<Recorded> record;
    while (budget && frame.status==metavm::Status::Running) {
        auto pc=frame.pc;const auto &instruction=s.program.code()[static_cast<std::size_t>(pc)];
        if (record.empty()) {
            auto found=s.traces.find(pc);
            if (found!=s.traces.end()) {
                auto before=frame.steps;
                auto why=osr_enter(s.program,frame,pc,budget,found->second.enter);
                auto executed=frame.steps-before;
                if (!executed) throw std::logic_error("MetaJIT: native trace made no progress");
                budget-=executed;s.stats.native+=executed;++s.stats.osr_entries;
                if (why==NativeExit::Guard) ++s.stats.guard_exits;
                if (why==NativeExit::Budget) ++s.stats.budget_exits;
                if (why==NativeExit::Trapped) ++s.stats.trap_exits;
                continue;
            }
        }
        bool start=record.empty() && s.options.native && native_supported() && s.headers[pc] && !s.disabled[pc] &&
            s.traces.size()<s.options.max_traces;
        if (start && s.heat[pc]<s.options.hot_threshold) ++s.heat[pc];
        start=start && s.heat[pc]>=s.options.hot_threshold;
        if (instruction.op==Op::Call) {
            if (!record.empty()) { s.publish(record);record.clear(); }
            if (start) s.disabled[pc]=true;
            start=false;
        }
        bool recording=start || !record.empty();
        auto before=frame.steps;
        try { metavm::run(s.program,frame,1,host); }
        catch (...) { s.stats.interpreted+=frame.steps-before;throw; }
        --budget;++s.stats.interpreted;
        if (recording) {
            auto next=frame.status==metavm::Status::Trapped ? pc+1 : frame.pc;
            record.push_back({pc,next});
            if (frame.status!=metavm::Status::Running || frame.pc==record.front().pc ||
                record.size()>=s.options.max_trace_instructions) {
                s.publish(record);record.clear();
            }
        }
    }
    if (!record.empty()) s.publish(record);
    return frame.status==metavm::Status::Returned ? metavm::Stop::Returned :
        frame.status==metavm::Status::Trapped ? metavm::Stop::Trapped : metavm::Stop::Budget;
}
} // namespace metajit
