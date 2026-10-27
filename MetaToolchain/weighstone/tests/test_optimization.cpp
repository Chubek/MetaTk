#include <Weighstone/CCINT.hpp>
#include <iostream>
#include <stdexcept>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error("test failed: " #condition); } while (false)
using namespace weighstone::ccint;
template<class Exception,class F> void throws(F f) {
    bool caught=false;
    try {f();} catch(const Exception&) {caught=true;}
    CHECK(caught);
}

static void tensors() {
    const auto layout=TensorLayout::row_major(TensorShape({2,3,4}),5);
    CHECK(layout.offset({1,2,3})==28);
    CHECK(layout.storage_range()==Interval(5,28));
    CHECK(layout.is_row_major_contiguous());
    CHECK(layout.reshape(TensorShape({6,4})).offset({5,3})==28);
    const auto transposed=layout.permute({2,0,1});
    CHECK(transposed.offset({3,1,2})==layout.offset({1,2,3}));
    CHECK(!transposed.is_row_major_contiguous());
    throws<std::invalid_argument>([&]{transposed.reshape(TensorShape({24}));});
    throws<std::invalid_argument>([&]{layout.reshape(TensorShape({25}));});
    throws<std::invalid_argument>([&]{layout.permute({0,0,2});});
    throws<std::invalid_argument>([&]{layout.offset({0});});
    throws<std::out_of_range>([&]{layout.offset({0,3,0});});
    throws<std::out_of_range>([&]{layout.offset({-1,0,0});});
    throws<std::invalid_argument>([]{TensorShape invalid({-1});});
    throws<std::invalid_argument>([]{TensorLayout invalid(TensorShape({2}),{});});

    const auto scalar=TensorLayout::row_major(TensorShape({}),9);
    CHECK(scalar.shape().elements()==1 && scalar.offset({})==9);
    const auto broadcast=scalar.broadcast_to(TensorShape({2,3}));
    CHECK(broadcast.offset({1,2})==9 && broadcast.storage_range()==Interval::point(9));
    CHECK(!broadcast.is_row_major_contiguous());
    CHECK(TensorShape({0,3}).broadcast(TensorShape({1,3}))==TensorShape({0,3}));
    CHECK(TensorShape({2,1,3}).broadcast(TensorShape({4,3}))==TensorShape({2,4,3}));
    throws<std::invalid_argument>([]{TensorShape({0}).broadcast(TensorShape({2}));});
    throws<std::invalid_argument>([&]{layout.broadcast_to(TensorShape({2,3,5}));});
    const auto empty=TensorLayout::row_major(TensorShape({2,0,3}));
    CHECK(empty.storage_range().empty());
    CHECK(empty.reshape(TensorShape({0})).shape().elements()==0);
    throws<std::out_of_range>([&]{empty.offset({0,0,0});});
    const TensorLayout reverse(TensorShape({4}),{-2},6);
    CHECK(reverse.offset({3})==0 && reverse.storage_range()==Interval(0,6));
    CHECK(!reverse.is_row_major_contiguous());
    const TensorLayout singleton(TensorShape({2,1,3}),{3,-99,1});
    CHECK(singleton.is_row_major_contiguous());
    const BigInt huge=power_of_two(130);
    CHECK(TensorShape({huge,huge}).elements()==huge*huge);
    CHECK(TensorLayout::row_major(TensorShape({huge,2})).offset({BigInt(huge-1),1})==huge*2-1);

    const auto i=LinearExpr::variable("i"),j=LinearExpr::variable("j");
    const auto domain=LoopNest({"i","j"},{{0,2},{0,3}}).domain();
    const AffineMap access({"i","j"},{i,j});
    CHECK(prove_in_bounds(domain,access,TensorShape({2,3})).is_proven());
    CHECK(prove_in_bounds(domain,access,TensorShape({1,3})).decision==Decision::unknown);
    CHECK(prove_in_bounds(domain,AffineMap({"i","j"},{i+LinearExpr(4),j}),TensorShape({2,3})).is_disproven());
    CHECK(prove_in_bounds(LoopNest({"i","j"},{{0,0},{0,3}}).domain(),access,TensorShape({0,0})).is_proven());
    const auto flat=TensorLayout::row_major(TensorShape({2,3})).linearize(access);
    CHECK(flat.eval({{"i",1},{"j",2}})==std::vector<BigInt>{5});
    throws<std::invalid_argument>([&]{prove_in_bounds(domain,access,TensorShape({3}));});

    // Exact storage hull and permutation invariance for signed/zero strides.
    for(int a=-3;a<=3;++a) for(int b=-3;b<=3;++b) {
        const TensorLayout view(TensorShape({2,3}),{a,b},7);
        BigInt lo=view.offset({0,0}),hi=lo;
        for(int x=0;x<2;++x) for(int y=0;y<3;++y) {
            const BigInt offset=view.offset({x,y});
            lo=std::min(lo,offset); hi=std::max(hi,offset);
            CHECK(offset==view.permute({1,0}).offset({y,x}));
        }
        CHECK(view.storage_range()==Interval(lo,hi));
    }
}

static void graphs() {
    using weighstone::support::DirectedGraph;
    CHECK(DirectedGraph{}.topological_order()->empty());
    CHECK(DirectedGraph{}.strongly_connected_components().empty());
    DirectedGraph graph(4);
    graph.add_edge(0,1); graph.add_edge(1,2); graph.add_edge(2,1); graph.add_edge(2,3);
    graph.add_edge(0,1);
    CHECK(graph.successors(0).size()==1);
    CHECK(!graph.topological_order());
    CHECK((graph.strongly_connected_components()==std::vector<std::vector<std::size_t>>{{0},{1,2},{3}}));
    CHECK(graph.reachable(0,3) && !graph.reachable(3,0));
    CHECK(graph.add_vertex()==4);
    throws<std::out_of_range>([&]{graph.add_edge(0,5);});
    throws<std::out_of_range>([&]{graph.reachable(5,0);});
    // Exhaustive directed 3-vertex graphs checked against transitive closure.
    for(unsigned mask=0;mask<512;++mask) {
        DirectedGraph g(3); bool reach[3][3]{};
        for(unsigned i=0;i<3;++i) for(unsigned j=0;j<3;++j)
            if(mask&(1u<<(i*3+j))) {g.add_edge(i,j); reach[i][j]=true;}
        for(unsigned k=0;k<3;++k) for(unsigned i=0;i<3;++i) for(unsigned j=0;j<3;++j)
            reach[i][j]=reach[i][j] || (reach[i][k] && reach[k][j]);
        const bool cycle=reach[0][0]||reach[1][1]||reach[2][2];
        const auto order=g.topological_order(); CHECK(order.has_value()==!cycle);
        if(order) {
            std::size_t position[3]{};
            for(std::size_t i=0;i<3;++i) position[(*order)[i]]=i;
            for(std::size_t i=0;i<3;++i) for(auto j:g.successors(i)) CHECK(position[i]<position[j]);
        }
        std::size_t component[3]{}; const auto components=g.strongly_connected_components();
        for(std::size_t c=0;c<components.size();++c) for(auto v:components[c]) component[v]=c;
        for(unsigned i=0;i<3;++i) for(unsigned j=0;j<3;++j) {
            CHECK(g.reachable(i,j)==(i==j || reach[i][j]));
            CHECK((component[i]==component[j])==(i==j || (reach[i][j] && reach[j][i])));
        }
    }
    DirectedGraph chain(20000);
    for(std::size_t i=1;i<chain.size();++i) chain.add_edge(i-1,i);
    CHECK(chain.strongly_connected_components().size()==chain.size());

    DependenceGraph dependencies(2,2);
    dependencies.add_edge(0,1,std::vector<BigInt>{1,-1});
    CHECK(dependencies.permutation_legal({0,1}).is_proven());
    CHECK(dependencies.permutation_legal({1,0}).is_disproven());
    CHECK(dependencies.parallel_axis(0).is_disproven());
    CHECK(dependencies.parallel_axis(1).is_proven());
    dependencies.add_edge(1,1);
    CHECK(dependencies.permutation_legal({0,1}).decision==Decision::unknown);
    CHECK(dependencies.parallel_axis(1).decision==Decision::unknown);
    CHECK(dependencies.permutation_legal({1,0}).is_disproven());
    throws<std::invalid_argument>([&]{dependencies.permutation_legal({0,0});});
    throws<std::invalid_argument>([&]{dependencies.add_edge(0,1,std::vector<BigInt>{1});});
    throws<std::out_of_range>([&]{dependencies.parallel_axis(2);});
    DependenceGraph local(1,2); local.add_edge(0,0,std::vector<BigInt>{0,0});
    CHECK(local.permutation_legal({1,0}).is_proven() && local.parallel_axis(0).is_proven());
}

static void loops() {
    // Tiling changes loop structure, so compare complete iteration sequences,
    // including descending, empty, nondivisible and arbitrary-precision cases.
    for(int start=-4;start<=4;++start) for(int stop=-4;stop<=4;++stop)
        for(int step=-3;step<=3;++step) if(step) for(int size=1;size<=4;++size) {
            const LoopRange loop(start,stop,step);
            std::vector<BigInt> original,tiled;
            for(int value=start;step>0?value<stop:value>stop;value+=step) original.push_back(value);
            CHECK(loop.trip_count()==original.size());
            for(BigInt t=0;t<loop.tile_count(size);++t) {
                const auto tile=loop.tile(t,size);
                CHECK(tile.trip_count()>0 && tile.trip_count()<=size);
                for(BigInt k=0;k<tile.trip_count();++k) tiled.push_back(tile.at(k));
            }
            CHECK(original==tiled);
        }
    const LoopRange large(0,power_of_two(200),3);
    CHECK(large.trip_count()==ceil_div(power_of_two(200),3));
    CHECK(large.tile(0,4).trip_count()==4);
    throws<std::invalid_argument>([]{LoopRange invalid(0,10,0);});
    throws<std::invalid_argument>([]{LoopRange(0,10).tile_count(0);});
    throws<std::out_of_range>([]{LoopRange(0,0).tile(0,2);});
    throws<std::out_of_range>([]{LoopRange(0,2).at(2);});
    const LoopNest nest({"i","j"},{{5,-1,-2},{0,3}});
    CHECK(nest.domain().integer_points().points.size()==9);
    CHECK((nest.induction_values().eval({{"i",2},{"j",1}})==std::vector<BigInt>{1,1}));

    const auto i=LinearExpr::variable("i"),j=LinearExpr::variable("j");
    const auto domain=LoopNest({"i"},{{0,5}}).domain();
    const std::vector<AffineAccess> recurrence{
        {"A",AffineMap({"i"},{i-LinearExpr(1)}),false},
        {"A",AffineMap({"i"},{i}),true}};
    const auto dependent=analyze_dependences(domain,recurrence);
    CHECK(dependent.complete.is_proven() && dependent.graph);
    CHECK(dependent.graph->edges().size()==4);
    CHECK(dependent.graph->parallel_axis(0).is_disproven());
    CHECK(dependent.graph->permutation_legal({0}).is_proven());
    const auto independent=analyze_dependences(domain,{{"A",AffineMap({"i"},{i}),true}});
    CHECK(independent.graph && independent.graph->parallel_axis(0).is_proven());
    CHECK(analyze_dependences(domain,recurrence,{{4,4096},1000}).complete.decision==Decision::unknown);
    CHECK(!analyze_dependences(domain,recurrence,{{100,4096},44}).graph);
    CHECK(analyze_dependences(domain,recurrence,{{100,4096},45}).graph.has_value());
    CHECK(!analyze_dependences(domain,{{"",AffineMap({"i"},{i}),true}}).graph);
    CHECK(!analyze_dependences(Polyhedron({"i"}),recurrence).graph);
    CHECK(analyze_dependences(Polyhedron({"i"}),{}).graph.has_value());
    CHECK(analyze_dependences(LoopNest({"i"},{{0,0}}).domain(),recurrence,{{0,0},0}).graph->edges().empty());
    const auto anti=analyze_dependences(domain,{{"A",AffineMap({"i"},{i+LinearExpr(1)}),false},
                                              {"A",AffineMap({"i"},{i}),true}});
    CHECK(anti.graph->edges().size()==4 && anti.graph->edges().front().source==0);
    const auto output=analyze_dependences(domain,{{"A",AffineMap({"i"},{LinearExpr(0)}),true}});
    CHECK(output.graph->edges().size()==10);
    const auto reads=analyze_dependences(domain,{{"A",AffineMap({"i"},{LinearExpr(0)}),false}});
    CHECK(reads.graph->edges().empty());
    const auto separate=analyze_dependences(domain,{{"A",AffineMap({"i"},{LinearExpr(0)}),false},
                                                  {"B",AffineMap({"i"},{i}),true}});
    CHECK(separate.graph->edges().empty());
    const auto two=LoopNest({"i","j"},{{0,3},{0,3}}).domain();
    const auto skewed=analyze_dependences(two,{
        {"A",AffineMap({"i","j"},{i-LinearExpr(1),j+LinearExpr(1)}),false},
        {"A",AffineMap({"i","j"},{i,j}),true}});
    CHECK(skewed.graph->permutation_legal({0,1}).is_proven());
    CHECK(skewed.graph->permutation_legal({1,0}).is_disproven());
}

int main() {
    tensors(); graphs(); loops();
    std::cout << "Tensor, graph and loop optimization tests passed\n";
}
