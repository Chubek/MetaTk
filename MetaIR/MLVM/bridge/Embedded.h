#ifndef MLVM_BRIDGE_EMBEDDED_H
#define MLVM_BRIDGE_EMBEDDED_H
#include <stddef.h>
typedef struct mlvm_library_source {
    const char *name;
    const unsigned char *source;
    size_t length;
} mlvm_library_source;
const mlvm_library_source *mlvm_embedded_libraries(size_t *count);
const unsigned char *mlvm_embedded_entry(size_t *length);
#endif
