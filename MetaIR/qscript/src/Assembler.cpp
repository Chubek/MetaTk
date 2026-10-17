#include "qscript/QScript.h"
namespace qscript {
Result<std::vector<std::uint8_t>> assemble(std::string_view source,bool object,Options o) {
    return metachain::Toolchain(o.toolchain).assemble(source,object);
}
} // namespace qscript
