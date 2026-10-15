#include "LinearIR.hpp"
namespace metachain::linear {
Code untyped_three_address(const metail::Program& p) {
    auto r=quadruples(p);
    for (auto& q:r.instructions) { q.result.type=Type::None; q.first.type=Type::None; q.second.type=Type::None; }
    return r;
}
} // namespace metachain::linear
