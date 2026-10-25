#ifndef METAVECT_SCENE_H
#define METAVECT_SCENE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Retained scene interchange ABI v1. Coordinates use points, x right, y down.
 * Views borrow immutable storage from mv_scene and expire at mv_scene_destroy.
 * No source evaluation, filesystem access, or backend-specific data. */
typedef enum mv_path_op { MV_MOVE, MV_LINE, MV_CUBIC, MV_CLOSE } mv_path_op;
typedef struct mv_path_command { mv_path_op op; double values[6]; } mv_path_command;
typedef struct mv_paint { int enabled; double red, green, blue; } mv_paint;
typedef struct mv_path_view {
    const mv_path_command *commands;
    size_t count;
    mv_paint stroke, fill;
    double stroke_width;
} mv_path_view;
typedef struct mv_scene_view {
    double width, height;
    const mv_path_view *paths;
    size_t count;
} mv_scene_view;
typedef struct mv_scene mv_scene;
/* Returns NULL on error; writes a NUL-terminated diagnostic when capacity > 0.
 * source need not be NUL-terminated. error may be NULL only with capacity == 0. */
mv_scene *mv_scene_parse(const char *source, size_t length, char *error, size_t capacity);
mv_scene_view mv_scene_get_view(const mv_scene *scene);
void mv_scene_destroy(mv_scene *scene);
#ifdef __cplusplus
}
#endif
#endif
