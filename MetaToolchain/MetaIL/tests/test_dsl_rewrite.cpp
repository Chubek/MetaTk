#include "MetaIL.hpp"
#include <iostream>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (0)
template<class F> void rejects(F fn, const std::string &message) {
    try { fn(); } catch (const metail::Error &error) {
        CHECK(std::string(error.what()).find(message)!=std::string::npos);return;
    }
    throw std::runtime_error("expected rejection: "+message);
}

int main(int argc, char **argv) {
    try {
        using namespace metail;
        CHECK(argc>=2);
        Context context(argv[1]);
        auto x=var("x"), y=var("y");
        auto rules=::dsl::rewrite_set(
            ::dsl::rule<"add-zero">(
                [](const auto &n) { return n.tag()=="add" && n.child(1)==integer(0).ast(); },
                [](const auto &n) { return n.child(0); }),
            ::dsl::rule<"multiply-one">(
                [](const auto &n) { return n.tag()=="mul" && n.child(1)==integer(1).ast(); },
                [](const auto &n) { return n.child(0); }));
        // Preserve the retained subtree exactly once, including its traps/effects.
        auto source=(x+0)*1;
        auto reduced=rewrite(source,rules);
        CHECK(reduced.expression.same_as(x));
        CHECK(reduced.applications==2 && reduced.passes==2);
        CHECK(source.same_as((x+0)*1));
        CHECK(rewrite(x,rules,{1,0}).expression.same_as(x));
        rejects([&] { rewrite(source,rules,{0,10}); },"pass limit");
        rejects([&] { rewrite(source,rules,{10,1}); },"multiply-one");
        rejects([&] { rewrite(source,rules,{1,10}); },"convergence");
        rules.max_iterations=0; // Adapter uses its own explicit budget.
        CHECK(rewrite(source,rules).expression.same_as(x));
        auto identity=::dsl::rewrite_set(::dsl::rule<"identity">(
            [](const auto &) { return true; }, [](const auto &n) { return n; }));
        CHECK(rewrite(source,identity,{1,0}).applications==0);
        CHECK(rewrite(source,::dsl::rewrite_set(),{1,0}).expression.same_as(source));
        auto toggle=::dsl::rewrite_set(::dsl::rule<"toggle">(
            [](const auto &n) { return n.tag()=="var"; },
            [](const auto &n) { return var(n.value()=="x" ? "y" : "x").ast(); }));
        rejects([&] { rewrite(x,toggle,{4,100}); },"convergence");
        auto invalid=::dsl::rewrite_set(::dsl::rule<"bad-arity">(
            [](const auto &) { return true; }, [](const auto &) { return ::dsl::node<"add">(); }));
        rejects([&] { rewrite(x,invalid); },"bad-arity");
        CHECK(x.same_as(var("x")));
        auto repair=::dsl::rule<"repair">([](const auto &) { return true; },
            [](const auto &) { return integer(1).ast(); });
        auto invalid_then_repair=::dsl::rewrite_set(std::get<0>(invalid.rules),repair);
        rejects([&] { rewrite(x,invalid_then_repair); },"bad-arity");

        auto leaf=::dsl::leaf<"int">("1");leaf.children().push_back(x.ast());
        rejects([&] { Expr::from_ast(leaf); },"leaf has children");
        auto callee=::dsl::leaf<"callee">("tick");callee.children().push_back(x.ast());
        rejects([&] { Expr::from_ast(::dsl::node<"call">(callee)); },"callee leaf");
        rejects([&] { Expr::operation(std::string("add\0suffix",10),{x,y}); },"NUL");
        Expr deep=x;for (int i=0;i<255;++i) deep=-deep;
        auto grow=::dsl::rewrite_set(::dsl::rule<"grow">(
            [](const auto &n) { return n.tag()=="var"; },
            [](const auto &n) { return ::dsl::node<"neg">(n); }));
        rejects([&] { rewrite(deep,grow); },"IR limits");
        rejects([&] { deep.substitute({{"x",-x}}); },"IR limits");
        std::vector<::dsl::ASTNode> huge(200000,x.ast());
        huge.insert(huge.begin(),::dsl::leaf<"callee">("tick"));
        rejects([&] { Expr::from_ast(::dsl::ASTNode("call",std::move(huge))); },"IR limits");

        auto substitution=(x+y).substitute({{"x",y+1},{"y",integer(2)}});
        CHECK(substitution.same_as((y+1)+2));
        CHECK(x.substitute({{"x",y},{"y",x}}).same_as(y));
        CHECK(x.substitute({{"x",x+1}}).same_as(x+1));
        CHECK(call("x",{x}).substitute({{"x",y}}).same_as(call("x",{y})));
        rejects([&] { x.substitute({{"bad-name",y}}); },"identifier");
        bool saw_callee=false;
        auto metadata_rule=::dsl::rewrite_set(::dsl::rule<"metadata">(
            [&](const auto &n) { saw_callee|=n.tag()=="callee";return false; },
            [](const auto &n) { return n; }));
        rewrite(call("tick",{x}),metadata_rule);CHECK(!saw_callee);

        auto visited=call("tick",{x+1});std::vector<std::string> tags;
        auto record=[&](const ::dsl::ASTNode &n) { tags.emplace_back(n.tag()); };
        auto visitor=::dsl::derive_visitor(visited.ast(),::dsl::on<"call">(record),
            ::dsl::on<"callee">(record),::dsl::on<"add">(record),
            ::dsl::on<"var">(record),::dsl::on<"int">(record));
        visited.visit(visitor);
        CHECK((tags==std::vector<std::string>{"call","callee","add","var","int"}));
        tags.clear();visited.visit(visitor,::dsl::VisitOrder::Postorder);
        CHECK((tags==std::vector<std::string>{"callee","var","int","add","call"}));

        Module module;
        module.function("all",{"x"}).label("entry").let("y",x+0).eval(call("tick")+0)
            .assume(x+0).branch(x+0,"yes","no").label("yes").ret(x+0)
            .label("no").jump("yes");
        auto rewritten=rewrite_expressions(module,rules,{10,5});
        CHECK(rewritten.expressions==5 && rewritten.applications==5);
        context.check(rewritten.module);
        CHECK(module.functions[0].statements[1].expr->same_as(x+0));
        CHECK(rewritten.module.functions[0].statements[1].expr->same_as(x));
        rejects([&] { rewrite_expressions(module,rules,{10,4}); },"application limit");
        CHECK(module.functions[0].statements[1].expr->same_as(x+0));

        // Emit pairs for the Lua semantic checker, also when the text parser is disabled.
        Module corpus;
        std::vector<Expr> cases={x,integer(INT64_MIN),integer(INT64_MAX),call("tick"),
            call("first")+call("second"),integer(1)/0,integer(INT64_MIN)/-1,
            integer(1)<<64,integer(1)>>-1,integer(1)%0,
            select(x,call("yes"),call("no")),integer(INT64_MAX)+1};
        for (std::size_t i=0;i<cases.size();++i) {
            auto before=(cases[i]+0)*1;auto after=rewrite(before,rules).expression;
            CHECK(after.same_as(cases[i]));
            auto name=std::to_string(i);
            corpus.function("before_"+name,{"x"}).ret(before);
            corpus.function("after_"+name,{"x"}).ret(after);
        }
        std::cout<<context.emit(corpus);
    } catch (const std::exception &error) { std::cerr<<error.what()<<'\n';return 1; }
}
