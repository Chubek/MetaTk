# 05 — Formats and plugin loading

The format selects commands, not a binary `.fmt` image. `plain` creates an
`Engine` with its native primitives and scripting commands. `latex` loads
`metatex_latex`; `nekotex` loads both `metatex_latex` and `metatex_nekotex`.
Add MetaVect independently with `--plugin metavect`.

```sh
build/metatex -f latex --plugin metavect -T html -o vector.html examples/vector.ltx
```

On Linux, plugin filenames are `metatex_NAME.so`. The loader searches beside
the executable, the adjacent installation's `lib/metatex` directory, the
working directory, then directories in colon-separated `METATEX_PLUGIN_PATH`.
Duplicate requested names are loaded once. Plugin names accept letters,
digits, underscores, and hyphens.

Every plugin exports `metatex_plugin_id` and `metatex_register_plugin`. Its
identity must match the requested name. The loader retains the shared library
until after the engine's registered handlers are destroyed. A plugin and core
must have compatible C++ ABIs even though their entry points use C linkage.

The shell launchers `mtex`, `mlatex`, and `nlatex` select the three formats and
forward arguments. `METATEX_BINARY` can point them at a nonstandard build.
Adding `\usepackage{metavect}` to source acknowledges an already loaded
MetaVect plugin; the source declaration does not load a shared library.

[Contents](README.md)
