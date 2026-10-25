#include "KernelRegistry.hpp"
#include <iostream>

int main() {
    try {
        metail::Context context;
        metail::Module module("controlflow");
        auto x=metail::var("x");
        module.function("choose",{"x"})
            .label("entry").branch(x,"yes","no")
            .label("yes").let("result",x+0).jump("join")
            .label("no").let("result",0).jump("join")
            .label("join").ret(metail::var("result"));
        context.check(module);
        metail::KernelRegistry registry(context);
        metail::KernelQuery query;
        query.tags={"control-flow"};query.kind="analysis";
        const auto kernels=registry.search(query);
        std::cout<<context.analyze(module,metail::KernelRegistry::passes(kernels));
        std::cout<<context.emit(context.optimize(module).module);
        return 0;
    } catch (const std::exception &error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
