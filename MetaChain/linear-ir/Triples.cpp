#include "LinearIR.hpp"
namespace metachain::linear {
std::vector<Triple> triples(const Code& c) {
    std::vector<Triple> r;
    for (const auto& q:c.instructions) r.push_back({q.op,q.first,q.second,q.result});
    return r;
}
Code from_triples(const std::vector<Triple>& c) {
    Code r; for (const auto& t:c) r.instructions.push_back({t.op,t.destination,t.first,t.second}); return r;
}
} // namespace metachain::linear
