#ifndef GPPX_SINK_API_H
#define GPPX_SINK_API_H
/** \file GPPX-Sink.h
 * Sink callbacks receive exact bytes and may be used by `#sink NAME expr`.
 */
#include "GPPX.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Write entire byte sequence to file (append != 0 means append).
 * Does not perform atomic rename. Returns 0 only if close succeeds as well.
 */
GPPX_API int gppx_sink_write_file(const char *path,const void *bytes,size_t size,int append);
/** A stock sink callback: userdata points to a live GPPX_FileSinkConfig. */
typedef struct GPPX_FileSinkConfig {const char *path;int append;} GPPX_FileSinkConfig;
GPPX_API int gppx_sink_file_callback(GPPX_State *,const char *destination,
                                      const char *data,size_t size,void *userdata);
static inline int gppx_sink_register(GPPX_State *state,const char *name,
                                     GPPX_SinkFn fn,void *userdata) {
    return gppx_register_sink(state,name,fn,userdata);
}
#ifdef __cplusplus
}
#endif
#endif
