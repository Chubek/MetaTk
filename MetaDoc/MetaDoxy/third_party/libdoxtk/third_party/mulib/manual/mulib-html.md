# MuLIB escaped HTML builder

**Header:** `include/mulib-html.h` · **Language:** ISO C11, C++17 declarations

## Scope and dependencies

Build escaped HTML with the owned XML DOM. **There is no HTML/HTML5 parser** and
no browser tree-construction algorithm. Do not feed arbitrary HTML to the XML
parser and expect HTML semantics. The HTML serializer understands void elements,
explicit end tags, document doctypes and escaped ordinary text/attributes.

Tag/attribute names must start with a lowercase ASCII letter and then contain
only lowercase letters, digits, `-`, `_`, or `:`. This supports ordinary HTML and
custom elements, not an SVG/MathML foreign-content serializer. Use `mulib-svg.h`
for SVG. CDATA nodes serialize as ordinary escaped text, not CDATA markup.

Void elements are `area base br col embed hr img input link meta param source
track wbr`. They serialize without closing tags and must have no children.
`script style xmp iframe noembed noframes plaintext noscript` are rejected: their
browser parsing contexts do not obey ordinary text escaping. HTML-specific
hazardous comment spellings also fail serialization. This is not a content-model
validator; callers must build sensible HTML (e.g. text-only title/textarea).

Implementation switches, explicitly defined in exactly one TU:

```c
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_HTML
#include "mulib-html.h"
```

All escaping, node/attribute allocation and lifetime management come from XML;
there is no second DOM or independent HTML entity implementation.

## API and ownership

| API | Contract |
| --- | --- |
| `mulib_html_document(title)` | Allocate an XML document containing `html/head/title` and `body`. NULL title means empty. Caller owns the document. |
| `mulib_html_body(document)` | Borrow the immediate body child of an html-rooted document, or NULL. |
| `mulib_html_add(parent, tag, text)` | Append a named element and, if non-NULL, a copied text child. Returns a borrowed element; NULL means no change to parent. Use NULL text for void elements. |
| `mulib_html_escape(out, text, length, attribute)` | Forward to shared XML escaping; 1/0. Never use in raw-text/script/style contexts. Source must not alias the destination buffer. |
| `mulib_html_serialize(node, length)` | Allocate NUL-terminated HTML. Documents require an `html` root and receive `<!doctype html>\n`; other nodes are fragments. |

Use XML APIs for attributes, child traversal, detachment and destruction. All
content must be XML-valid UTF-8; this deliberately excludes embedded NUL and
invalid controls. Serializers enforce XML's 128-element nesting limit. NULL
returns signal invalid arguments/content, depth overflow or allocation failure;
lengths exclude NUL and are zero on failure. Free output with `free`.

For `pre`, `textarea` and legacy `listing`, a leading LF is serialized with one
extra LF because HTML parsers discard the first LF immediately after the start
tag. Empty initial text/CDATA nodes are skipped when deciding whether padding
is needed; a preceding element/comment prevents padding. This preserves content
such as `"\nx"` after HTML parsing without changing the DOM.

**Escaping is not sanitization.** Event-handler attributes, dangerous `href`/`src`
schemes and application-specific attribute policies remain the caller's concern.
The builder prevents text from becoming ordinary markup; it is not a safe policy
layer for arbitrary user-supplied tags/attributes, CSS or URLs.

## Runnable example

```c
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_HTML
#include "mulib-html.h"

int main(void) {
    mulib_xml_node_t *doc=mulib_html_document("A & B");
    char *html;
    assert(doc);
    assert(mulib_html_add(mulib_html_body(doc),"p","<escaped> & text"));
    assert(mulib_html_add(mulib_html_body(doc),"br",NULL));
    html=mulib_html_serialize(doc,NULL);
    assert(html&&strstr(html,"<p>&lt;escaped&gt; &amp; text</p><br>"));
    free(html); mulib_xml_destroy(doc);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/html.c -o ./mulib-html
./mulib-html
```

`tests/test_markup.c` verifies shared escaping, void/raw-text rejection,
document output and cleanup across allocation failures.
