module MetaVect_Scene;
// C ABI v1; returned views borrow storage until mv_scene_destroy.
extern(C):
enum mv_path_op : int { MV_MOVE, MV_LINE, MV_CUBIC, MV_CLOSE }
struct mv_path_command { mv_path_op op; double[6] values; }
struct mv_paint { int enabled; double red, green, blue; }
struct mv_path_view {
    const(mv_path_command)* commands;
    size_t count;
    mv_paint stroke, fill;
    double stroke_width;
}
struct mv_scene_view {
    double width, height;
    const(mv_path_view)* paths;
    size_t count;
}
struct mv_scene;
mv_scene* mv_scene_parse(const(char)* source, size_t length, char* error, size_t capacity);
mv_scene_view mv_scene_get_view(const(mv_scene)* scene);
void mv_scene_destroy(mv_scene* scene);
