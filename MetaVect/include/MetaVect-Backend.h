#ifndef METAVECT_BACKEND_H
    #define METAVECT_BACKEND_H
    #ifdef __cplusplus
    extern "C" {
    #endif
    typedef struct mv_backend_info { const char *name; const char *description; unsigned capabilities; } mv_backend_info;
const mv_backend_info *mv_backend_query(void);
    #ifdef __cplusplus
    }
    #endif
    #endif
