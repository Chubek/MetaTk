#include "LinearIR.hpp"

namespace metachain::linear {
// Triples: operation plus up to two argument operands with a retained
// destination. Unlike classical triples the destination is explicit, so the
// form round-trips losslessly and never implies SSA.
std::vector<Triple> triples(const Code& c) {
    std::vector<Triple> r;
    r.reserve(c.instructions.size());
    for (const auto& q:c.instructions) r.push_back({q.op,q.first,q.second,q.result});
    return r;
}
Code from_triples(const std::vector<Triple>& c) {
    Code r;
    r.instructions.reserve(c.size());
    for (const auto& t:c) r.instructions.push_back({t.op,t.destination,t.first,t.second});
    return r;
}
} // namespace metachain::linear
