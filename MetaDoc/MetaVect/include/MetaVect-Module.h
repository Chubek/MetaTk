#ifndef METAVECT_MODULE_H
    #define METAVECT_MODULE_H
    #ifdef __cplusplus
    extern "C" {
    #endif
    typedef struct mv_module_info { const char *name; unsigned abi_version; } mv_module_info;
const mv_module_info *mv_module_query(void);
    #ifdef __cplusplus
    }
    #endif
    #endif
