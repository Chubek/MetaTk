#include "LinearIR.hpp"

namespace metachain::linear {
// Pseudo-assembly: canonical textual MetaIL for the given linear IR.
// Throws std::invalid_argument naming the first bad operand when the
// input does not convert; callers needing diagnostics use to_metail().
std::string pseudo_assembly(const Code& c) {
    auto p=to_metail(c);
    if (!p) throw std::invalid_argument(p.diagnostics.front().message);
    return metail::emit(p.program);
}
} // namespace metachain::linear
