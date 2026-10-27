# 31 — MetaVect graphics

Load the MetaVect plugin with `--plugin metavect`. It accepts versioned retained
scene artifacts and creates vector-graphic document blocks. The same geometry
becomes inline SVG in HTML and native paths in both PDF backends.

<!-- metatex-example {"format":"plain","plugins":["metavect"],"text":"A circle."} -->
```tex
\metavect[width=120]{
metavect-scene 1 120 80;
stroke 0.1 0.2 0.4; fill 0.7 0.8 1; width 2;
circle 60 40 30;
}{A circle.}
```

The two required arguments are scene source and plain-text caption. Width is
optional, measured in points, and must be finite and positive when specified.
Use `\metavectfile[width=120]{drawing.mvs}{Caption}` for a file; the path is
relative to the containing TeX source. Scene and caption are not macro-expanded.

Scene v1 supports rectangles, circles, lines, compound paths, and cubic curves
with RGB fill/stroke paints. Coordinates start at the canvas's top-left.
PDF fitting preserves aspect ratio and clips paths to the scene canvas. JSON
retains geometry and paints; text output retains the caption.

This interface accepts `.mvs` artifacts, not arbitrary `.mvct` programs or SVG
markup. Gradients, opacity, transforms, and scene text objects are outside the
current scene contract. See [the detailed integration guide](../docs/METAVECT.md)
for formats and native scene construction.

[Contents](README.md)
