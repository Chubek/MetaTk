# MuLIB SVG builder

**Header:** `include/mulib-svg.h` · **Language:** ISO C11, C++17 declarations

## Scope and dependencies

Create SVG documents/shapes in the shared XML DOM and serialize them with the
**same XML serializer/escaping**. No renderer, geometry solver, path parser,
SVG validator or sanitizer is included. Generic XML attributes support extension
without a separate attribute store.

Explicit implementation switches in one TU:

```c
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_SVG
#include "mulib-svg.h"
```

Only ISO C11 library APIs are used. Geometry must be finite. Numbers use 17-digit
`g` formatting, with the locale decimal separator normalized to `.` without
changing the process locale. Applications must not concurrently change the C
locale during number formatting. No mathematical runtime functions requiring a
separate `-lm` link are called by this module.

## API

All shape functions append to the given XML element parent and return a borrowed
shape node; they return NULL without changing the parent on failure. Use the SVG
root or a group as parent. Inputs are copied, including all string attributes.

| API | Contract |
| --- | --- |
| `mulib_svg_document(width, height)` | Owned XML document with `svg` root, SVG namespace, width/height and `viewBox="0 0 W H"`. Positive finite dimensions required. |
| `mulib_svg_group(parent, id)` | Append `g`; optional copied id. |
| `mulib_svg_rect(parent, x, y, width, height, fill)` | Append rectangle; finite coordinates and nonnegative size. |
| `mulib_svg_circle(parent, cx, cy, radius, fill)` | Append circle; finite center/nonnegative radius. |
| `mulib_svg_line(parent, x1, y1, x2, y2, stroke, stroke_width)` | Append line; finite coordinates/nonnegative width. |
| `mulib_svg_text(parent, x, y, text, fill)` | Append text element with copied XML-valid UTF-8 content. NULL text is invalid; markup characters are escaped. |
| `mulib_svg_path(parent, data, fill, stroke)` | Append path; non-NULL data is escaped as the `d` attribute, **not parsed/validated as path grammar**. |
| `mulib_svg_serialize(document_or_svg, length)` | Require an `svg` root and delegate to XML serialization. Returns malloc-owned NUL-terminated text or NULL. |

`fill`/`stroke`/`id` arguments may be NULL to omit the attribute. These attributes
are not interpreted as paint/CSS/ID syntax. Use `mulib_xml_set_attr` for stroke
styles, transforms, rounded corners, accessibility metadata, etc.; application
semantics remain the caller's responsibility. General XML APIs can deliberately
construct invalid SVG; the builder does not enforce SVG content models.

Use `mulib_xml_root` to borrow the root and `mulib_xml_destroy` to free the
document/subtree. Attribute/value pointers are borrowed until replaced/destroyed.
Shape failure is atomic with respect to the parent. Serialization can fail on
allocation or XML's 128-element depth limit; optional output length excludes NUL
and is zero on failure. Free output with `free`.

Escaping prevents XML markup injection through text/attribute values; it does
not sanitize SVG URLs, event attributes, CSS or arbitrary nodes added through
the XML API. No filesystem/network access is performed.

## Runnable example

```c
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_SVG
#include "mulib-svg.h"

int main(void) {
    mulib_xml_node_t *doc=mulib_svg_document(200,100);
    mulib_xml_node_t *root;
    char *svg;
    assert(doc); root=mulib_xml_root(doc);
    assert(mulib_svg_circle(root,50,50,20,"blue"));
    assert(mulib_svg_text(root,80,55,"A < B & C","black"));
    svg=mulib_svg_serialize(doc,NULL);
    assert(svg&&strstr(svg,"A &lt; B &amp; C"));
    free(svg); mulib_xml_destroy(doc);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/svg.c -o ./mulib-svg
./mulib-svg
```

`tests/test_markup.c` checks shape output, XML round trips, escaping, nonfinite
and negative geometry rejection, and cleanup on allocation failures.
