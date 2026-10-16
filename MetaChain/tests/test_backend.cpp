#include "MetaChain.hpp"
#include "graph-ir/Superblocks.hpp"
#include "linear-ir/LinearIR.hpp"
#include "satie/BackendSAT.hpp"
#include <cstdlib>
#include <iostream>
#include <random>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
int main() {
    using namespace metachain;
    backend::Graph tree{{{"const",{},6,false},{"const",{},7,false},{"mul",{0,1},0,false}},{2}};
    std::vector<isel::Rule> rules{{"constant",{"const",{}},1},{"multiply",{"mul",{{"*",{}},{"*",{}}}},3},{"folded",{"mul",{{"const",{}},{"const",{}}}},2}};
    for (auto selection:{isel::tree_cover(tree,rules),isel::burs(tree,rules),isel::dag_cover(tree,rules),isel::graph_cover(tree,rules),isel::smt_cover(tree,rules)}) {
        CHECK(selection); CHECK(selection.optimal); CHECK(selection.cost==2); CHECK(selection.tiles.size()==1);
    }
    CHECK(isel::macro_cover(tree,rules));
    CHECK(!isel::tree_cover(tree,{}));
    auto shared=tree; shared.nodes.push_back({"mul",{0,2},0,false}); shared.outputs={3};
    CHECK(!isel::tree_cover(shared,rules)); CHECK(isel::dag_cover(shared,rules));
    CHECK(isel::dag_cover(shared,rules).cost==isel::smt_cover(shared,rules).cost);
    backend::Graph repeated{{{"const",{},7,false},{"mul",{0,0},0,false}},{1}};
    auto repeated_cover=isel::dag_cover(repeated,rules); CHECK(repeated_cover&&repeated_cover.cost==2&&repeated_cover.tiles.size()==1);
    auto cycle=tree; cycle.nodes[0].inputs={2}; CHECK(!isel::graph_cover(cycle,rules));
    auto bad=tree; bad.outputs={100}; CHECK(!isel::dag_cover(bad,rules));
    isel::Options tiny; tiny.max_search=0; CHECK(!isel::dag_cover(tree,rules,tiny));
    auto malformed_rule=rules; malformed_rule[0].pattern={"*",{}}; CHECK(!isel::dag_cover(tree,malformed_rule));
    backend::Graph deep; deep.nodes.resize(1024,backend::Node{"const",{},0,false});
    tiny.max_search=512; CHECK(!isel::dag_cover(deep,rules,tiny));
    auto zero_cost=rules; zero_cost[2].cost=0; CHECK(isel::smt_cover(tree,zero_cost).cost==0);

    std::vector<isched::Task> tasks{{{},2,"alu",true},{{},1,"alu",true},{{{0,2},{1,1}},1,"alu",true}};
    isched::Options machine; machine.issue_width=2; machine.resources["alu"]=2;
    for (auto s:{isched::list_based(tasks,machine),isched::cycle_driven(tasks,machine),isched::static_schedule(tasks,machine),isched::constraint(tasks,machine)}) CHECK(isched::valid(tasks,s,machine));
    auto exact=isched::constraint(tasks,machine); CHECK(exact.optimal&&exact.makespan==3);
    auto invalid=tasks; invalid[0].dependencies={{2,1}}; CHECK(!isched::list_based(invalid));
    CHECK(isched::speculative(tasks,{{2,0}},machine));
    invalid=tasks; invalid[2].speculatable=false; CHECK(!isched::speculative(invalid,{{2,0}},machine));
    auto traces=isched::trace_schedule({{{1,2},{0.9,0.1}},{{2},{1}},{{},{}}}); CHECK(traces.size()==1&&traces[0].size()==3);
    auto delay=isched::delay_slots({{{{}, {1},false,false},false,false},{{{0},{},true,false},true,false},{{},false,true}});
    CHECK(delay&&delay.order[2]==0&&!delay.order[0]);
    CHECK(!isched::delay_slots({{{},true,false}}));
    auto trapped_delay=isched::delay_slots({{{{}, {1},false,false},false,false},{{{}, {},false,true},false,false},{{{0},{},true,false},true,false},{{},false,true}});
    CHECK(trapped_delay&&trapped_delay.order[0]==0&&!trapped_delay.order[3]);

    regalloc::InterferenceGraph interference; interference.neighbors.resize(4);
    interference.add_edge(0,1); interference.add_edge(1,2); interference.add_edge(2,0); interference.add_edge(2,3);
    for (auto r:{regalloc::graph_coloring(interference,2),regalloc::briggs(interference,2)}) { CHECK(regalloc::valid(interference,r,2)); CHECK(!r.spills.empty()); }
    interference.precolored={{0,0}}; CHECK(regalloc::valid(interference,regalloc::briggs(interference,3),3));
    interference.precolored={{0,0},{1,0}}; CHECK(!regalloc::graph_coloring(interference,3));
    auto scan=regalloc::linear_scan({{0,0,10},{1,1,2},{2,2,3}},1); CHECK(scan&&scan.spills.count(0)&&scan.registers[1]&&scan.registers[2]);
    CHECK(!regalloc::linear_scan({{0,2,1}},1));
    CHECK(!regalloc::linear_scan({{0,0,1},{0,2,3}},1));
    CHECK(regalloc::linear_scan({{0,0,1}},std::numeric_limits<unsigned>::max()).registers[0]==0);

    auto p=metail::parse("const r0, 4\nconst r1, 1\nsub r0, r0, r1\njnz r0, 2\nhalt r0\n"); CHECK(p);
    auto cfg=graph::cfg(p.program); CHECK(cfg&&cfg.blocks.size()==3); CHECK(graph::dominators(cfg)[2].count(0));
    auto data=graph::dfg(p.program); CHECK(data&&data.graph.nodes.size()==p.program.size());
    CHECK(!graph::dag(data.graph));
    auto ssa=graph::gssa(p.program); CHECK(ssa&&!ssa.phis.empty()&&backend::verify(ssa.graph).empty());
    CHECK(graph::value_graph(p.program)); CHECK(graph::pdg(p.program));
    CHECK(!graph::pdw(data.graph).empty()); CHECK(graph::vsdg(p.program)); CHECK(graph::sea_of_nodes(p.program)); CHECK(!graph::superblocks(cfg).empty());
    auto live=regalloc::liveness(p.program); CHECK(live&&!live.intervals.empty());
    CHECK(regalloc::valid(live.interference,regalloc::graph_coloring(live.interference,4),4));
    for (auto code:{linear::quadruples(p.program),linear::typed_three_address(p.program),linear::untyped_three_address(p.program)}) {
        CHECK(code); auto roundtrip=linear::to_metail(code); CHECK(roundtrip&&roundtrip.program.code==p.program.code);
        CHECK(linear::to_metail(linear::from_triples(linear::triples(code))).program.code==p.program.code);
        CHECK(linear::pseudo_assembly(code)==metail::emit(p.program));
    }
    auto malformed=linear::quadruples(p.program); malformed.instructions[0].first.kind=linear::Operand::Kind::None; CHECK(!linear::to_metail(malformed));
    malformed=linear::quadruples(p.program); malformed.instructions[0].first.type=linear::Type::Label; CHECK(!linear::to_metail(malformed));
    auto calls=metail::parse("call 2\nhalt r0\nret\n"); CHECK(calls); CHECK(graph::cfg(calls.program)); CHECK(!graph::gssa(calls.program));
    auto endless=metail::parse("jmp 0\n"); CHECK(endless); CHECK(!graph::pdg(endless.program));

    // Check sequential cardinality against all assignments, including k=0/n.
    for (unsigned count=1;count<=5;++count) for (unsigned k=0;k<=count;++k) for (unsigned mask=0;mask<(1u<<count);++mask) {
        sat::Formula f; std::vector<int> vars; for (unsigned n=0;n<count;++n) vars.push_back(f.fresh());
        f.at_most(vars,k); unsigned set=0;
        for (unsigned n=0;n<count;++n) { bool bit=mask&(1u<<n); f.clauses.push_back({bit?vars[n]:-vars[n]}); set+=bit; }
        CHECK(sat::solve(f).satisfiable==(set<=k));
    }
    std::mt19937 random(42);
    // Differentially compare minimum-cost search with the SAT encoding.
    for (unsigned test=0;test<40;++test) {
        backend::Graph g;
        for (unsigned n=0;n<6;++n) {
            if (n>1&&random()%2) g.nodes.push_back({"mul",{random()%n,random()%n},0,false});
            else g.nodes.push_back({"const",{},static_cast<std::int64_t>(random()%10),false});
        }
        auto heuristic=isel::graph_cover(g,rules),sat_result=isel::smt_cover(g,rules);
        CHECK(heuristic&&sat_result&&heuristic.optimal&&sat_result.optimal&&heuristic.cost==sat_result.cost);
    }
    // Small exhaustive schedules independently establish the optimal horizon.
    for (unsigned test=0;test<20;++test) {
        std::vector<isched::Task> work(4);
        for (unsigned n=0;n<work.size();++n) {
            work[n].latency=1+random()%2;
            for (unsigned pred=0;pred<n;++pred) if (random()%2) work[n].dependencies.push_back({pred,work[pred].latency});
        }
        isched::Options m; m.issue_width=2; auto scheduled=isched::constraint(work,m); CHECK(isched::valid(work,scheduled,m));
        unsigned optimum=100;
        for (unsigned encoding=0;encoding<4096;++encoding) {
            unsigned number=encoding; std::array<unsigned,4> cycle{}; bool valid=true; unsigned span=0;
            std::array<unsigned,8> issued{};
            for (unsigned n=0;n<4;++n) { cycle[n]=number%8; number/=8; if (++issued[cycle[n]]>2) valid=false; span=std::max(span,cycle[n]+work[n].latency); }
            for (unsigned n=0;n<4;++n) for (auto dep:work[n].dependencies) if (cycle[n]<cycle[dep.predecessor]+dep.delay) valid=false;
            if (valid) optimum=std::min(optimum,span);
        }
        CHECK(scheduled.optimal&&scheduled.makespan==optimum);
    }
    for (unsigned test=0;test<30;++test) {
        regalloc::InterferenceGraph g; g.neighbors.resize(12);
        for (unsigned a=0;a<12;++a) for (unsigned b=a+1;b<12;++b) if (random()%3==0) g.add_edge(a,b);
        g.moves={{0,3},{3,6},{6,9},{2,8}};
        CHECK(regalloc::valid(g,regalloc::graph_coloring(g,3),3)); CHECK(regalloc::valid(g,regalloc::briggs(g,3),3));
    }
    // Backend diagnostics and analysis helpers.
    {
        auto st=backend::stats(tree); CHECK(st.nodes==3&&st.edges==2&&st.outputs==1);
        CHECK(backend::dump(tree).find("mul")!=std::string::npos);
        CHECK(backend::to_dot(tree).find("digraph")!=std::string::npos);
        CHECK(backend::live_from_outputs(tree).size()==3);
        CHECK(!backend::find_cycle(tree));
        CHECK(backend::find_cycle(cycle));
        CHECK(isel::pattern_size(rules[2].pattern)==3&&isel::pattern_operands(rules[1].pattern)==2&&isel::pattern_depth(rules[2].pattern)==2);
        CHECK(isel::dump_rule(rules[0]).find("constant")!=std::string::npos);
        auto sel=isel::dag_cover(tree,rules); CHECK(isel::verify_cover(tree,sel));
        CHECK(isel::dump_result(tree,rules,sel).find("cost=")!=std::string::npos);
        CHECK(!isel::index_rules(rules).empty());
        auto sched=isched::list_based(tasks,machine);
        CHECK(isched::critical_path(tasks)<=sched.makespan);
        auto sst=isched::schedule_stats(tasks,sched); CHECK(sst.tasks==3&&sst.makespan==sched.makespan);
        CHECK(isched::dump_schedule(tasks,sched).find("makespan=")!=std::string::npos);
        auto alloc=regalloc::briggs(interference,3);
        auto ast=regalloc::alloc_stats(interference,alloc); CHECK(ast.values==4);
        CHECK(regalloc::dump_allocation(interference,alloc,3).find("allocation")!=std::string::npos);
        CHECK(graph::dump_cfg(cfg).find("B0")!=std::string::npos);
        CHECK(graph::dump_analysis(data).find("graph")!=std::string::npos);
        auto code=linear::quadruples(p.program);
        CHECK(linear::dump_code(code).find("const")!=std::string::npos);
        CHECK(linear::dump_operand(code.instructions[0].result)[0]=='r');
    }
    std::cout << "backend tests passed\n";
}
