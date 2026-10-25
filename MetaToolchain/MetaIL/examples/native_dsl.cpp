#include "MetaIL.hpp"
#include <iostream>

int main() {
    try {
        using namespace metail;
        Context context;
        auto x=var("x");
        // Native C++ operators and DSLtk AST construction interoperate.
        auto identity=Expr::from_ast(::dsl::node<"add">(x.ast(),::dsl::leaf<"int">("0")));
        Module module("native");
        module.function("scale",{"x"}).ret(select(x>0,identity*8,-x));
        auto optimized=context.optimize(module,"AddZeroRight,MultiplyByPowerOfTwoRight03");
        std::cout<<context.emit(optimized.module);
        std::cout<<optimized.module.functions.at(0).statements.at(0).expr->dump()<<'\n';
        // Text enters the same structured representation and can be edited.
        auto parsed=context.parse("fn answer() { return 40; }");
        parsed.functions.at(0).statements.at(0).expr=integer(42);
        std::cout<<context.emit(parsed);
        return 0;
    } catch (const std::exception &error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
