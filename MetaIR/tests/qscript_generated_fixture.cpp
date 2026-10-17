#include "qscript/include/QScript-Generated.h"

// Pull the public context and generated-runtime APIs out of the static archive.
// Production consumers link MetaChain::QScript directly through CMake instead.
extern "C" qs_runtime* qscript_generated_fixture(qs_context* context, const qs_runtime_config* config) {
    if (!context) {
        auto* temporary = qs_create();
        qs_destroy(temporary);
    }
    return qs_runtime_create(context, config);
}
