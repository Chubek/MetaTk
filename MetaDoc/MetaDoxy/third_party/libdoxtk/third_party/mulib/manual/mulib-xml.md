# MuLIB XML

**Header:** `include/mulib-xml.h` · **Language:** ISO C11, C++17 declarations

## Scope and dependencies

An owned UTF-8 DOM, a length-delimited XML parser, and an escaped compact
serializer. Node kinds are document, element, text, comment and CDATA. The parser
accepts the five predefined entities (`amp`, `lt`, `gt`, `apos`, `quot`) and
decimal/hexadecimal numeric references to XML 1.0 characters. Input may have a
UTF-8 BOM and an initial XML 1.0 declaration. Declaration fields must be ordered
`version`, optional `encoding` (`UTF-8` or `utf-8`), optional `standalone`.
Declarations/BOMs are not retained in the DOM.

This is a focused XML subset, not a namespace-aware validating processor:

- Names use `[A-Za-z_:][A-Za-z0-9_.:-]*`. Colons/namespace declarations are stored
  literally; namespace URIs are not resolved. Unicode content is supported,
  but Unicode names are not.
- DTDs, entity declarations, external entities, non-UTF-8 encodings and other
  processing instructions are rejected. **No filesystem/network I/O occurs.**
- There must be exactly one document element. Outside it, only literal
  whitespace and comments are allowed. Duplicate attributes and mismatched tags
  are errors.
- Literal CR/CRLF normalize to LF, including comments and CDATA. Literal
  attribute tabs/newlines normalize to spaces; character references retain their
  referenced character. Adjacent text/CDATA nodes are not merged.
- Parser and serializers accept at most `MULIB_XML_DEPTH_LIMIT` (128) nested
  elements. Builders can construct deeper trees; serialization then fails.
  Destruction is iterative even for such trees.

Implementation switches, **all explicitly defined before includes in one TU**:

```c
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#include "mulib-xml.h"
```

Dependencies are `mulib-strbuf.h` and `mulib-utf8.h`; no implementation switch is
defined implicitly. Other translation units include declarations without switches.

## Ownership and errors

`mulib_xml_new` and `mulib_xml_parse` return owned nodes. `mulib_xml_append`
transfers ownership **only on success**; it rejects already-attached nodes,
cycles, document children, and illegal document-level content. Attributes and
node values are copied. `mulib_xml_detach` returns subtree ownership to the
caller. `mulib_xml_destroy` automatically detaches its subtree, then frees all
descendants and attributes. NULL destruction is harmless.

Accessors return borrowed pointers. Attribute replacement invalidates its old
value pointer; destruction invalidates all pointers into the destroyed subtree.
Use `free` on serialized strings. Strings cannot contain embedded NUL. The
parser's explicit byte length must not include a C terminator.

Pointer-returning operations return NULL on malformed data, invalid arguments,
depth overflow or allocation failure. Mutators return 1/0 and leave ownership
unchanged on failure. Parse errors optionally fill `mulib_xml_error_t` with a
byte offset and static message; successful parsing clears the error. Whole-input
character-validation errors have offset zero. No `errno` contract is provided.
Serializer lengths exclude NUL and are zero on failure.

## API

| API | Contract |
| --- | --- |
| `mulib_xml_valid_name(name)` | Check supported ASCII name grammar. |
| `mulib_xml_valid_text(text, length)` | Check UTF-8 and XML 1.0 character ranges; NULL/0 is valid, NUL characters are not. |
| `mulib_xml_new(type, value)` | Element: name; text/comment/CDATA: content; document: NULL or empty. Rejects `--`/trailing `-` in comments and `]]>` in CDATA. |
| `mulib_xml_append(parent, child)` | Append to element/document; return 1 on transfer. |
| `mulib_xml_detach(node)` / `mulib_xml_destroy(node)` | Take back ownership / release subtree. |
| `mulib_xml_type(node)` | Node kind; NULL returns `MULIB_XML_DOCUMENT`. |
| `mulib_xml_name(node)` / `mulib_xml_value(node)` | Element name / non-container content; NULL for inapplicable kinds. |
| `mulib_xml_parent`, `mulib_xml_first`, `mulib_xml_next` | Borrow tree links; NULL at boundaries. |
| `mulib_xml_root(document)` | First/only element root; NULL for non-documents or empty documents. |
| `mulib_xml_set_attr(node, name, value)` | Transactionally copy/set; preserve insertion order when replacing. |
| `mulib_xml_attr(node, name)` | Borrow value or NULL; an existing empty value is `""`, not NULL. |
| `mulib_xml_attr_count`, `mulib_xml_attr_name`, `mulib_xml_attr_value` | Enumerate insertion-order attributes; out-of-range access returns NULL. |
| `mulib_xml_parse(source, length, error)` | Parse a complete document. |
| `mulib_xml_escape(out, text, length, attribute)` | Append escaped text to a strbuf. |
| `mulib_xml_serialize(node, length)` | Allocate compact XML; a document requires a root, other nodes serialize as fragments. |

The escape helper is shared by HTML, Markdown and SVG. It escapes `&`, `<`, `>`
and CR; attribute mode also escapes both quotes, LF and tab. It validates input
before appending, but allocation failure can leave a prefix in `out`. Source
must not alias `out->data`, because strbuf growth can relocate storage.
Document-level whitespace is instead emitted literally: character references
are not legal outside the root. Its CR/CRLF therefore normalize to LF on parsing.

Mutable DOMs require external synchronization. Parsing has bounded recursion;
large input/tree sizes remain limited by available memory, not a streaming API.

## Runnable example

```c
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#include "mulib-xml.h"

int main(void) {
    const char input[]="<r a='&quot;'>Hello &amp; XML</r>";
    mulib_xml_error_t error;
    mulib_xml_node_t *doc=mulib_xml_parse(input,sizeof(input)-1,&error);
    char *output;
    assert(doc&&!error.message);
    assert(!strcmp(mulib_xml_attr(mulib_xml_root(doc),"a"),"\""));
    output=mulib_xml_serialize(doc,NULL);
    assert(output&&strstr(output,"Hello &amp; XML"));
    free(output); mulib_xml_destroy(doc);
    return 0;
}
```

Build the extended example:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/xml.c -o ./mulib-xml
./mulib-xml
```

`tests/test_markup.c` covers malformed encodings, forbidden DTD/entities,
normalization, nesting limits, ownership cycles, and allocation-failure cleanup.
