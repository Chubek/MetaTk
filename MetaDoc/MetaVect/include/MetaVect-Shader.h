#ifndef METAVECT_SHADER_H
    #define METAVECT_SHADER_H
    #ifdef __cplusplus
    extern "C" {
    #endif
    typedef struct mv_shader_info { const char *name; unsigned version; } mv_shader_info;
const mv_shader_info *mv_shader_query(void);
    #ifdef __cplusplus
    }
    #endif
    #endif
