#include "KernelRegistry.hpp"
#include <iostream>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (0)
template<class F> static void rejects(F fn) {
    bool rejected=false;
    try { fn(); } catch (const metail::Error &) { rejected=true; }
    CHECK(rejected);
}
int main(int argc, char **argv) {
    try {
        CHECK(argc==3);
        metail::Context context(argv[1]);
        metail::KernelRegistry registry(context);
        CHECK(registry.size()==321 && registry.search().size()==321);
        CHECK(registry.find("AddZeroRight") && !registry.find("addzeroright"));
        const auto file=registry.request(std::string("AddZeroRight"));
        CHECK(file.contents.find("add($x,0)")!=std::string::npos);
        CHECK(file.kernel.path.is_absolute());
        metail::KernelQuery query;
        query.keywords="MULTIPLY power";query.tags={"strength-reduction","rewrite"};query.use="SHIFTS";
        auto matches=registry.search(query);
        CHECK(!matches.empty());
        CHECK(registry.paths(query).size()==matches.size());
        CHECK(registry.request(query).size()==matches.size());
        CHECK(context.optimize("fn f(x) { return x * 8; }",registry.passes(matches)).source.find("<< 3")!=std::string::npos);
        query.tags.push_back("nonexistent");CHECK(registry.search(query).empty());
        query={};query.tags={"numeric","statistics"};query.kind="ANALYSIS";
        CHECK(registry.search(query).size()==24);
        query={};query.keywords="add($x,0)";query.kind="rewrite";
        CHECK(registry.search(query).size()==1);
        rejects([&] { registry.request(std::string("missing")); });
        rejects([&] { registry.passes({}); });
        rejects([&] { registry.passes(registry.entries()); });
        rejects([&] { context.check("fn f() { return @; }"); });
        rejects([&] { context.check(std::string("fn f() {}\0ignored",17)); });
        context.check("fn ok() { return 1; }");
        auto moved=std::move(context);
        rejects([&] { context.check("fn f() {}"); });
        CHECK(moved.emit("fn f() { return 1; }").find("return 1")!=std::string::npos);

        // A registered name need not match the filename. The actual DSL declaration
        // supplies discovery metadata and the real path, including spaces.
        const auto temp=std::filesystem::path(argv[2]);
        std::filesystem::create_directories(temp);
        const auto custom=temp/"different name.optk";
        { std::ofstream out(custom);out<<R"optk(local K = lkopt.optimizer.new "CustomIdentity"
K:rewrite { pattern = "add($x,0)", replace = "$x" }
K:metadata { description = "Custom embedded pass", keywords = {"widget"},
             tags = {"project"}, uses = {"reduce widget cost"} }
)optk"; }
        moved.load_kernels(temp.string());
        registry.refresh(moved);
        CHECK(registry.size()==322);
        query={};query.keywords="WIDGET";query.tags={"PROJECT"};query.use="reduce cost";
        matches=registry.search(query);
        CHECK(matches.size()==1 && matches[0].name=="CustomIdentity");
        CHECK(matches[0].path==std::filesystem::absolute(custom));
        CHECK(moved.optimize("fn f(x) { return x+0; }",registry.passes(matches)).source.find("return x;")!=std::string::npos);
        rejects([&] { moved.load_kernels(temp.string()); }); // duplicate is transactional
        CHECK(metail::KernelRegistry(moved).size()==322);
        metail::Context independent(argv[1]);CHECK(metail::KernelRegistry(independent).size()==321);
        std::filesystem::remove(custom);
        rejects([&] { registry.request(std::string("CustomIdentity")); });
        { std::ofstream out(custom);out<<R"optk(local K = lkopt.optimizer.new "RolledBack"
K:rewrite { pattern = "add($x,0)", replace = "$x" }
K:metadata { tags = {"bad\ntag"} }
)optk"; }
        rejects([&] { moved.load_kernels(temp.string()); });
        CHECK(metail::KernelRegistry(moved).size()==322);
        std::filesystem::remove(custom);
        std::filesystem::remove(temp);
        // The registry is independent of the runtime which created it.
        auto snapshot=[&] { metail::Context c(argv[1]);return metail::KernelRegistry(c); }();
        CHECK(!snapshot.request(std::string("ConstantFolding")).contents.empty());
        return 0;
    } catch (const std::exception &error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
