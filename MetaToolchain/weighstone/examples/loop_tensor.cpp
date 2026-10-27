#include <Weighstone/CCINT.hpp>
#include <iostream>

using namespace weighstone::ccint;

int main() {
    const auto i=LinearExpr::variable("i"),j=LinearExpr::variable("j");
    const LoopNest nest({"i","j"},{{0,4},{0,8}});
    const auto domain=nest.domain();
    const AffineMap access({"i","j"},{i,j});
    const auto tensor=TensorLayout::row_major(TensorShape({4,8}));
    if(!prove_in_bounds(domain,access,tensor.shape()).is_proven()) return 1;

    // Each iteration writes a different element of a known memory object.
    const auto analysis=analyze_dependences(domain,{{"A",tensor.linearize(access),true}});
    if(!analysis.graph || !analysis.graph->permutation_legal({1,0}).is_proven()) return 1;
    std::cout << "Interchange preserves memory dependences; A[3,7] offset = "
              << tensor.offset({3,7}) << '\n';

    const LoopRange loop(0,8);
    for(BigInt tile=0;tile<loop.tile_count(3);++tile) {
        const auto part=loop.tile(tile,3);
        std::cout << "Tile " << tile << ": " << part.trip_count() << " iterations\n";
    }
}
