#include "LinearIR.hpp"
namespace metachain::linear {
std::string pseudo_assembly(const Code& c) {
    auto p=to_metail(c);
    if (!p) throw std::invalid_argument(p.diagnostics.front().message);
    return metail::emit(p.program);
}
} // namespace metachain::linear
