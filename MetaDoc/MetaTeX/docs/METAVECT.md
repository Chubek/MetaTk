# MetaVect: vector visualization in MetaTeX

The `metatex_metavect` plugin embeds retained MetaVect scenes in documents.
HTML contains accessible, responsive inline SVG; both PDF devices draw native
vector paths with page fitting, clipping, and preserved aspect ratio. JSON retains
the geometry, paints, dimensions, and caption. Text output includes the caption.

## Run the example

From `MetaTeX/` after building:

```sh
build/metatex --plugin metavect -f latex -T html -o vector.html examples/vector.ltx
build/metatex --plugin metavect -f latex -T pdf -o vector.pdf examples/vector.ltx
build/metatex --plugin metavect -f latex -T json -o vector.json examples/vector.ltx
```

The plugin works with `plain`, `latex`, and `nekotex`. `--plugin` is repeatable;
duplicate names load once. LaTeX's `\usepackage{metavect}` acknowledges an already
loaded plugin; it does not load a library or an external `.sty` file.

## Commands

```tex
\metavect[width=240]{
metavect-scene 1 320 180;
stroke 0.1 0.2 0.4; fill 0.7 0.8 1; width 2;
rect 10 10 300 160;
fill none; circle 160 90 55;
path M 30 140 C 100 20 220 20 290 140;
}{A circle and a cubic Bezier curve.}

\metavectfile[width=240]{diagram.mvs}{External scene caption.}
```

Both commands require two brace arguments: scene/filename and plain-text caption.
`[width=POINTS]` is optional; natural scene width is the default. Width must be
finite and in `[0.01, 1000000]`. PDF devices further shrink artwork to fit the
page's drawable area. Captions follow the figure through ordinary text layout
and can continue on another page. An empty caption is permitted.

Scene paths are relative to the TeX file containing the command, including nested
`\input` files. Inline scenes use MetaTeX's normal argument scanner, so TeX `%`
comments apply. Scene numbers and paint components need no TeX escapes. Neither
the scene nor caption argument undergoes macro expansion.

## Scene format and integration boundary

The versioned `.mvs` format and C, C++, and D interfaces are defined in
[`MetaVect/scene/README.md`](../../MetaVect/scene/README.md).
It supports filled/stroked rectangles, cubic-approximated circles, compound
paths, lines, and cubic Bezier curves, using RGB paints and nonzero winding.
Paths are clipped to the canvas; coordinates use points with the origin at the
top left. Geometry stays vector-based throughout rendering.

MetaVect's `.mvct` evaluator, semantic pipeline, and MVIR lowering are still
unfinished. This plugin consumes **scene artifacts**, not arbitrary `.mvct`
programs, SVG markup, or executable code. A future MetaVect compiler backend can
emit this scene contract; current tools can produce it directly. `.mvct` files
are rejected with an explicit diagnostic. Scene v1 has no gradients, opacity,
text objects, transforms, or external resources; use document text for labels.

## Library use

```cpp
#include "MetaTeX.h"

metatex::Engine engine;
engine.registerCommand("diagram", [](metatex::Engine& e) {
    metavect::Scene scene("metavect-scene 1 120 80; circle 60 40 30;");
    e.addGraphic(std::move(scene), "A vector circle", 120);
});
auto document = engine.compile("\\diagram");
auto html = metatex::renderHTML(document);
```

`BlockKind::VectorGraphic` owns a shared immutable `metavect::Scene`. Backends
consume the typed scene in `Document`; they never parse the TeX or scene source.
Documents retain scene storage across subsequent `Engine::compile` calls.
Rebuild all C++ plugins with the updated headers/core; the C entry-point names
remain unchanged, but the public C++ structures have grown.

The build uses sibling `../MetaVect/scene` by default; set
`-DMETATEX_METAVECT_SOURCE_DIR=/path/to/MetaVect` for another source layout.
The scene library needs only the C++ standard library, not the D compiler.
`cmake --install build --prefix PREFIX` installs the plugin, scene headers,
and scene archive alongside MetaTeX. No external executable or shell is invoked.
