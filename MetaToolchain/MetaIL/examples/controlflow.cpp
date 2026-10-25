#include "KernelRegistry.hpp"
#include <iostream>

int main() {
    try {
        metail::Context context;
        const std::string source=R"(module controlflow;
fn choose(x) {
entry:
  br x, yes, no;
yes:
  let result = x + 0;
  goto join;
no:
  let result = 0;
  goto join;
join:
  return result;
})";
        context.check(source);
        metail::KernelRegistry registry(context);
        metail::KernelQuery query;
        query.tags={"control-flow"};query.kind="analysis";
        const auto kernels=registry.search(query);
        std::cout<<context.analyze(source,metail::KernelRegistry::passes(kernels));
        std::cout<<context.optimize(source).source;
        return 0;
    } catch (const std::exception &error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
