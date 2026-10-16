#include "LinearIR.hpp"

namespace metachain::linear {
// Untyped three-address code: identical shapes to quadruples with all type
// tags stripped. Round-trips through to_metail(), which accepts None-typed
// operands and rechecks shapes against the canonical form.
Code untyped_three_address(const metail::Program& p) {
    auto r=quadruples(p);
    for (auto& q:r.instructions) { q.result.type=Type::None; q.first.type=Type::None; q.second.type=Type::None; }
    return r;
}
} // namespace metachain::linear
