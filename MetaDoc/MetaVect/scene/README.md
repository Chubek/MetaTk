# Retained scene interchange, version 1

`metavect_scene` is a working, backend-independent scene parser with immutable
typed views. MetaTeX uses it for SVG and native PDF visualization. This is a
separate interchange component: it does not implement `.mvct` evaluation,
semantic checking, MVIR lowering, or any rewrite pass.

## Format (`.mvs`)

```text
metavect-scene 1 320 180;
stroke 0.1 0.2 0.4;
fill 0.8 0.9 1;
width 2;
rect 10 10 300 160;
fill none;
circle 160 90 55;
path M 30 140 C 100 20 220 20 290 140 L 300 150 Z;
```

Every statement ends with `;`. Whitespace separates tokens; newlines are optional.
The header is required exactly once. Unknown commands, extra operands, and
unsupported versions are errors with line numbers. Numbers use the classic
locale (decimal `.`); exponent notation is accepted. There are no comments,
strings, imports, filesystem actions, or source evaluation.

| Statement | Meaning |
|---|---|
| `metavect-scene 1 WIDTH HEIGHT;` | Canvas dimensions in points |
| `stroke R G B;` / `stroke none;` | Subsequent path outlines |
| `fill R G B;` / `fill none;` | Subsequent path fills |
| `width W;` | Subsequent stroke width in points |
| `rect X Y WIDTH HEIGHT;` | Closed rectangle |
| `circle X Y RADIUS;` | Four-cubic Bezier approximation of a circle |
| `path M X Y L X Y C X1 Y1 X2 Y2 X Y Z;` | General path; operations may repeat |

Coordinates have origin at top left, x increasing rightward, y downward. Canvas
clipping is the renderer's responsibility. RGB components must lie in `[0, 1]`.
Default paint is black stroke, no fill, width 1. Each path captures its current
paint independently; draw order is source order. Fill uses nonzero winding;
strokes use butt caps, miter joins, and miter limit 10. A path must start with
`M`; after `Z`, another subpath must start with `M`. Empty scenes are valid;
empty paths are errors. Styles without geometry do not create paths.

Limits: 4 MiB source, 10,000 paths, 100,000 total path operations, finite numeric
operands of absolute value at most 1,000,000. Dimensions, radii, and stroke widths
are at least 0.01 points. NUL bytes are rejected. The parser is iterative and
does not execute user code. No gradients, alpha, text, transforms, external
resources, or optimizer passes are part of v1.

## C ABI

Include `include/MetaVect-Scene.h`, and link CMake target `metavect_scene` (or
`metavect`, which links it transitively). A standalone C link command must also
link the platform C++ runtime. Allocation and destruction happen inside the same
library; exceptions never cross this ABI.

```c
char error[512];
const char source[] = "metavect-scene 1 120 80; circle 60 40 30;";
mv_scene *scene = mv_scene_parse(source, sizeof(source)-1, error, sizeof(error));
if (scene) {
    mv_scene_view view = mv_scene_get_view(scene);
    /* Visit view.paths, then each path's commands and paints. */
    mv_scene_destroy(scene);
}
```

On failure, parse returns NULL and writes a truncated, NUL-terminated diagnostic
when a nonzero buffer capacity is supplied. A NULL error pointer requires capacity
zero. `mv_scene_destroy(NULL)` is safe; `mv_scene_get_view(NULL)` returns an empty
zero-dimension view. All views borrow immutable scene storage and expire at
destruction. Independent scene instances can be parsed concurrently.

## C++ and D

`include/MetaVect-Scene.hpp` provides `metavect::Scene(std::string_view)` and
`Scene::view()`. Construction throws `std::runtime_error` on invalid input.
Copies share immutable storage through RAII ownership. It is also included by
`MetaVect.hpp`.

`include/MetaVect-Scene.di` defines module `MetaVect_Scene`, mirroring the C ABI,
including struct layouts and ownership. Pass this interface module to the D
compiler and link `metavect_scene` and the C++ runtime. The `.mvct` frontend does
not implicitly import or execute this module.

## Visualization

```sh
../MetaTeX/build/metatex --plugin metavect -f latex -T html \
  -o vector.html ../MetaTeX/examples/vector.ltx
```

See the [MetaTeX plugin guide](../../MetaTeX/docs/METAVECT.md) for embedding,
PDF export, captions, and scene-file resolution.
