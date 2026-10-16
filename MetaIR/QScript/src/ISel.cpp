#include "qscript/QScript.h"
namespace qscript {
Result<std::string> select(const metail::Program& p,Options o) {
    // MetaIL already encodes the concrete integer operations; use the existing
    // x86-64 lowering, including its runtime traps and execution limits.
    return metachain::Toolchain(o.toolchain).native_source(p);
}
} // namespace qscript
