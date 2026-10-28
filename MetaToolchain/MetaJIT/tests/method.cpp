#include "MethodBasedJIT.hpp"
#include <iostream>

namespace {
using Function = std::int64_t (*)();
void require(bool value) { if (!value) throw std::runtime_error("method JIT assertion failed"); }
template<class Exception, class F> void rejects(F fn) {
    try { fn(); } catch (const Exception &) { return; }
    throw std::runtime_error("expected method JIT rejection");
}
metajit::CodeBuffer constant(const std::string &name) {
    metajit::CodeBuffer buffer;
    // Readable prefix for instrumented indirect-call probes; ENDBR64 + mov eax,42; ret.
    buffer.append({0,0,0,0,0,0,0,0,0xf3,0x0f,0x1e,0xfa,0xb8,42,0,0,0,0xc3});
    buffer.entry(name,8,0);
    return buffer;
}
}
int main() {
    try {
        metajit::MethodBasedJIT jit;
        rejects<std::invalid_argument>([&] { jit.register_method("", [] { return constant("x"); }); });
        rejects<std::invalid_argument>([&] { jit.register_method("x", {}); });
        rejects<std::invalid_argument>([&] { jit.get<Function>("missing"); });
        int compilations=0;
        jit.register_method("answer", [&] { ++compilations; return constant("answer"); });
        require(!jit.compiled("answer"));
        rejects<std::invalid_argument>([&] { jit.register_method("answer", [] { return constant("answer"); }); });
        auto answer=jit.get<Function>("answer");
        require(answer()==42 && jit.get<Function>("answer")==answer && compilations==1);
        require(jit.compiled("answer"));
        rejects<std::invalid_argument>([&] { jit.get<std::int64_t (*)(std::int64_t)>("answer"); });
        int attempts=0;
        jit.register_method("retry", [&] {
            if (++attempts==1) throw std::runtime_error("compiler failure");
            return constant("retry");
        });
        rejects<std::runtime_error>([&] { jit.get<Function>("retry"); });
        require(!jit.compiled("retry"));
        require(jit.get<Function>("retry")()==42 && attempts==2);
        int signatures=0;
        jit.register_method("signature", [&] { ++signatures; return constant("signature"); });
        rejects<std::invalid_argument>([&] { jit.get<std::int64_t (*)(std::int64_t)>("signature"); });
        require(!jit.compiled("signature") && jit.get<Function>("signature")()==42 && signatures==2);
        jit.register_method("recursive", [&] { jit.get<Function>("recursive"); return constant("recursive"); });
        for (int i=0;i<2;++i) rejects<std::logic_error>([&] { jit.get<Function>("recursive"); });
        jit.register_method("wrong", [] { return constant("other"); });
        rejects<std::invalid_argument>([&] { jit.get<Function>("wrong"); });
        require(!jit.compiled("wrong") && answer()==42);
        std::cout << "lazy method compilation tests passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
