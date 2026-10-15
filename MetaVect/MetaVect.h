#ifndef METAVECT_H
#define METAVECT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define METAVECT_VERSION_MAJOR 0
#define METAVECT_VERSION_MINOR 1
#define METAVECT_VERSION_PATCH 0
typedef enum mv_status {
    MV_STATUS_OK = 0,
    MV_STATUS_INVALID_ARGUMENT = 1,
    MV_STATUS_OUT_OF_MEMORY = 2,
    MV_STATUS_NOT_IMPLEMENTED = 3
} mv_status;
const char *mv_version_string(void);
const char *mv_status_string(mv_status status);
#ifdef __cplusplus
}
#endif
#endif
