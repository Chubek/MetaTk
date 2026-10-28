#include "KernelRegistry.hpp"
#include <iostream>
#include <limits>
#include <type_traits>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (0)
template<class F> static void rejects(F fn) {
    bool rejected=false;try { fn(); } catch (const metail::Error &) { rejected=true; }CHECK(rejected);
}
int main(int argc, char **argv) {
    try {
        CHECK(argc>=2);
        using namespace metail;
        static_assert(!std::is_convertible_v<Expr,bool>);
        static_assert(!std::is_constructible_v<Expr,double>);
        Context context(argv[1]);auto x=var("x"),y=var("y");
        Module module("native");
        module.function("f",{"x"}).label("entry").ret((x+0)*8);
        auto optimized=context.optimize(module,"AddZeroRight,MultiplyByPowerOfTwoRight03");
        CHECK(optimized.module.functions[0].statements[1].expr->ast().tag()=="shl");
        CHECK(module.functions[0].statements[1].expr->ast().tag()=="mul");
        CHECK(optimized.stats.find("AddZeroRight")!=std::string::npos);
        CHECK(context.emit(optimized.module).find("return (x << 3);")!=std::string::npos);
        CHECK(context.analyze(module,"ControlFlowGraph").find("entry")!=std::string::npos);
        CHECK((2+x).ast().tag()=="add" && (0==x).ast().tag()=="eq");
        std::vector<Expr> expressions={x+y,x-y,x*y,x/y,x%y,x&y,x|y,x^y,x<<1,x>>1,
            x==y,x!=y,x<y,x<=y,x>y,x>=y,land(x,y),lor(x,y),-x,~x,!x,select(x,y,0),call("tick",{x,1})};
        Function all("all",{"x","y"});
        for (const auto &e:expressions) all.eval(e);
        all.assume(x>=0).ret(std::numeric_limits<std::int64_t>::min());
        Module operators;operators.add(std::move(all));context.check(operators);
        if (argc==2) {
            auto text=context.emit(operators);auto parsed=context.parse(text);
            CHECK(context.emit(parsed)==text);
            for (std::size_t i=0;i<expressions.size();++i)
                CHECK(parsed.functions[0].statements[i].expr->same_as(expressions[i]));
            CHECK(context.emit(optimized.module)==context.optimize(context.emit(module),"AddZeroRight,MultiplyByPowerOfTwoRight03").source);
            parsed.functions[0].statements.back().expr=integer(42);
            CHECK(context.emit(parsed).find("return 42;")!=std::string::npos);
        } else rejects([&] { context.parse("fn f() {}"); });
        Module effect;effect.function("effect").ret(call("tick")*0);
        CHECK(context.emit(context.optimize(effect,"MultiplyZeroRight").module).find("tick()")!=std::string::npos);
        Module foreign;
        foreign.extern_function("magnitude","c",{"i64"},"i64","labs");
        foreign.function("foreign",{"x"}).ret(call("magnitude",{x})*0);
        auto kept=context.optimize(foreign);
        CHECK(kept.module.externals.size()==1 && kept.module.externals[0].symbol=="labs");
        CHECK(context.emit(kept.module).find("magnitude(x)")!=std::string::npos);
        CHECK(context.linkage(foreign).find("\"arguments\":[\"-lc\"]")!=std::string::npos);
        if (argc==2) {
            CHECK(context.emit(context.parse(context.emit(foreign)))==context.emit(foreign));
            CHECK(context.linkage(context.emit(foreign))==context.linkage(foreign));
        }
        Module trap;trap.function("trap").ret((integer(1)/0)*0);
        CHECK(context.emit(context.optimize(trap,"MultiplyZeroRight").module).find("/ 0")!=std::string::npos);
        Module wrap;wrap.function("wrap").ret(integer(INT64_MAX)+1);
        auto wrapped=context.optimize(wrap,"ConstantFolding");
        CHECK(wrapped.module.functions[0].statements[0].expr->ast().value()=="-9223372036854775808");
        auto tree=::dsl::node<"add">(::dsl::leaf<"var">("x"),::dsl::leaf<"int">("0"));
        CHECK(Expr::from_ast(tree).same_as(x+0));
        rejects([&] { Expr::from_ast(::dsl::node<"add">(x.ast())); });
        rejects([&] { Expr::from_ast(::dsl::leaf<"int">("9223372036854775808")); });
        rejects([&] { Expr(UINT64_MAX); });
        rejects([&] { var("true"); });rejects([&] { call("add",{1,2}); });
        Expr deep=x;for (int i=0;i<255;++i) deep=-deep;
        rejects([&] { auto too_deep=-deep;(void)too_deep; });
        auto copy=module;copy.functions[0].statements[1].expr=integer(99);
        CHECK(module.functions[0].statements[1].expr->ast().tag()=="mul");
        Module invalid;invalid.function("f").jump("unknown");rejects([&] { context.check(invalid); });
        invalid.functions[0].statements.clear();invalid.functions[0].ret(1);context.check(invalid);
        Module lifetime=[&] { Context local(argv[1]);return local.optimize(module,"AddZeroRight").module; }();
        context.check(lifetime);
        KernelRegistry registry(context);KernelQuery q;q.keywords="MultiplyByPowerOfTwoRight03";
        CHECK(context.emit(context.optimize(module,registry.passes(registry.search(q))).module).find("<< 3")!=std::string::npos);
        return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
