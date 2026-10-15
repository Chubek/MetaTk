#include "metachain/Regalloc.hpp"
namespace metachain::regalloc {
Result graph_coloring(const InterferenceGraph& g,unsigned colors) { return color(g,colors); }
} // namespace metachain::regalloc
