#include "MetaVect.h"
const char *mv_version_string(void) { return "0.1.0"; }
const char *mv_status_string(mv_status status) {
    switch (status) {
    case MV_STATUS_OK: return "ok";
    case MV_STATUS_INVALID_ARGUMENT: return "invalid argument";
    case MV_STATUS_OUT_OF_MEMORY: return "out of memory";
    case MV_STATUS_NOT_IMPLEMENTED: return "not implemented";
    default: return "unknown status";
    }
}
