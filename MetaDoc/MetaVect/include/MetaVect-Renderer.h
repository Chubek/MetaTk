#ifndef METAVECT_RENDERER_H
    #define METAVECT_RENDERER_H
    #ifdef __cplusplus
    extern "C" {
    #endif
    typedef struct mv_render_options { double width; double height; double scale; } mv_render_options;
int mv_renderer_validate_options(const mv_render_options *options);
    #ifdef __cplusplus
    }
    #endif
    #endif
