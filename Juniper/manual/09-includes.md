# 9. Includes and project organization

Use includes to share macro definitions and backend profiles while keeping one
clear entry file. Include search is deterministic, but differs from many C/C++
projects: nested includes are **root/include-path-relative**, not relative to
the directory of the including file.

## A concrete multi-file project

The checked [include example](examples/includes/Juniperfile) has this layout:

```text
includes/
  Juniperfile
  macros/arithmetic.xpp
  macros/constants.xpp
```

`Juniperfile` contains:

```text
#include "macros/arithmetic.xpp"
@module(include_demo)
@fn(scale, (x))
  @return(@project_scale(x))
@end
```

Inside `macros/arithmetic.xpp`:

```text
#ifndef project_arithmetic_loaded
#define @project_arithmetic_loaded 1
#include "macros/constants.xpp"
#define @project_scale mul(#1, @project_factor)
#endif
```

Inside `macros/constants.xpp`:

```text
#define @project_factor 8
```

The nested include says `macros/constants.xpp`, not just `constants.xpp`, because
the root input directory remains `includes/` for this search.

```sh
"$J" check "$MANUAL_EXAMPLES/includes/Juniperfile"
"$J" emit "$MANUAL_EXAMPLES/includes/Juniperfile" \
  -o "$WORK/includes.junir" --depfile "$WORK/includes.d"
```

The depfile records the entry file, standard prelude, and files actually included
by the chosen conditional configuration.

## Search order

For relative includes Juniper searches:

1. The root input file's directory.
2. The standard library's directory.
3. Explicit `-I` directories, in argument order.

It does not implicitly search the process's working directory or system include
directories. Adding the same basename in an earlier search directory can change
which file is selected, so prefer project-prefixed paths over generic names.

```sh
"$J" emit "$WORK/Juniperfile" \
  -I "$WORK/shared-macros" -I /opt/project/juniper-macros \
  -o "$WORK/configured.junir"
```

Absolute includes are possible; for reproducible projects prefer stable include
roots and relative paths instead of machine-specific filenames.

## Separate definitions from declarations

A header containing only `#define` directives can usually be included in many
places. A header that emits a `backend` or `module` declaration must be included
in the right place and only once. Version/backend declarations precede the body;
a second backend is not a profile merge operation.

An include guard tests a **bare** name with `#ifndef`, then defines its `@` macro.
That asymmetry is intentional. The standard prelude is injected automatically;
you do not need to include `Juniper.xpp` in every entry file.

[Previous](08-macro-parameters.md) · [Index](README.md) · [Next](10-configuration.md)
