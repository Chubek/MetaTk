#include "metachain/Regalloc.hpp"

namespace metachain::regalloc {
// Chaitin-style graph coloring: repeatedly remove a low-degree node (or an
// optimistic spill candidate by cost/degree), then color in reverse order.
// Precolored nodes keep their fixed colors; uncolorable nodes spill to
// abstract slots owned by the caller.
Result graph_coloring(const InterferenceGraph& g,unsigned colors) {
    return color(g,colors);
}
} // namespace metachain::regalloc
