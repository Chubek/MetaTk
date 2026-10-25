#include "KernelRegistry.hpp"
#include <iostream>

int main() {
    try {
        metail::Context context;
        metail::KernelRegistry registry(context);
        metail::KernelQuery query;
        query.keywords="multiply power";
        query.tags={"strength-reduction"};
        query.use="shifts";
        query.kind="rewrite";
        for (const auto &file:registry.request(query)) {
            std::cout<<file.kernel.name<<" -- "<<file.kernel.path<<'\n'<<file.contents;
        }
        const auto passes=metail::KernelRegistry::passes(registry.search(query));
        std::cout<<context.optimize("fn scale(x) { return x * 8; }",passes).source;
        return 0;
    } catch (const std::exception &error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
