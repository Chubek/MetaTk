#include <juniper/Juniper.hpp>
#include <iostream>

int main() {
    juniper::Compiler compiler;
    auto module = compiler.parse("fn f(x,y,z) { return add(mul(x,y),z); }");
    if (!module) { std::cerr << juniper::format(*module.error()) << '\n'; return 1; }
    auto& backend = module.get()->backend;
    backend.name = "fused";
    using P = juniper::Pattern;
    backend.rules.push_back({"multiply_add", P::operation("add", {
        P::operation("mul", {P::reg(), P::reg()}), P::reg()}), "MADD", 1, 3, "alu"});
    auto selected = juniper::select(*module.get());
    if (!selected) return 1;
    auto scheduled = juniper::schedule(*selected.get());
    if (!scheduled) return 1;
    std::cout << juniper::emit_machine_json(*scheduled.get());
}
