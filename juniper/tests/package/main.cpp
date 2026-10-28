#include <juniper/Juniper.hpp>
#include <iostream>

int main(int argc, char** argv) {
    juniper::Compiler compiler(argc > 1 ? argv[1] : nullptr);
    auto module = compiler.parse("fn twice(x) { return mul(x,2); }");
    if (!module) return 1;
    auto optimized = compiler.optimize(*module.get(), "ConstantFolding");
    if (!optimized) return 1;
    auto selected = juniper::select(*optimized.get(), argc > 1 ? argv[1] : nullptr);
    if (!selected) return 1;
    auto scheduled = juniper::schedule(*selected.get());
    if (!scheduled) return 1;
    std::cout << juniper::emit_machine_json(*scheduled.get());
}
