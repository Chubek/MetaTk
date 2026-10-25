#include "MetaIL.hpp"
#include <iostream>

int main() {
    try {
        metail::Context context;
        // Euclid's loop for nonnegative inputs. This example emits/analyzes IR;
        // it does not claim to compile or execute native code.
        auto a=metail::var("a"),b=metail::var("b");
        metail::Module module("euclid");
        module.function("gcd",{"a","b"})
            .label("entry").jump("loop")
            .label("loop").branch(b!=0,"step","done")
            .label("step").let("remainder",a%b).let("a",b)
            .let("b",metail::var("remainder")).jump("loop")
            .label("done").ret(a+0);
        const auto result=context.optimize(module,"AddZeroRight,ConstantFolding");
        std::cout<<context.emit(result.module);
        std::cout<<context.analyze(result.module,"NaturalLoops,LiveVariables");
        std::cout<<result.stats;
        return 0;
    } catch (const std::exception &error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
