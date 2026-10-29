# MuLIB XPath-style queries

**Header:** `include/mulib-xpath.h` · **Language:** ISO C11, C++17 declarations

## Scope and dependencies

Select borrowed nodes from the **same DOM** used by XML, HTML and SVG. This is a
focused XPath-style evaluator, not a complete XPath 1.0/2.0 implementation.

Supported syntax:

- Absolute `/root/item` and relative `item`, `./item`, `../item` paths. `/` selects
  the owning document. Absolute queries on a detached element tree fail.
- `//` descendant traversal, e.g. `//item` or `/root//item`.
- Element names (same ASCII grammar as XML), `*` (elements), `node()` (all node
  kinds), `text()` (text and CDATA), and `comment()`.
- Sequential predicates: positive one-based `[N]`, `[@attr]`,
  `[@attr='literal']`, and `[text()='literal']`. Either quote style is allowed;
  string literals do not process backslash escapes or entity references.
- Whitespace around steps/predicates/operators.

Predicates apply per child selection, **not to the global result**. Thus
`//item[1]` selects the first `item` child of every parent. `item[@x][2]` first
filters by attribute, then chooses the second remaining sibling.
`text()='literal'` succeeds if any immediate text/CDATA child has exactly that
value; it does not concatenate descendant text. Matching is case-sensitive.
Results are unique and in document order, regardless of overlapping traversal.

No general axes, namespace resolution, unions, arithmetic, wildcards inside
names, arbitrary functions, attribute-valued results, variables, or string-value
coercions. `//.` and `//..` are explicitly rejected. Limits are 64 path steps and
8 predicates per step. Unsupported/malformed syntax fails even when a preceding
step matches no nodes.

Define every implementation switch in one translation unit:

```c
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_XPATH
#include "mulib-xpath.h"
```

No dependency implementation is enabled implicitly. No external libraries or I/O.

## API, ownership and failures

```c
typedef struct mulib_xpath_result {
    mulib_xml_node_t **nodes;
    size_t count;
} mulib_xpath_result_t;

int mulib_xpath_query(mulib_xml_node_t *context, const char *expression,
                     mulib_xpath_result_t *out, mulib_xml_error_t *error);
void mulib_xpath_result_free(mulib_xpath_result_t *result);
```

The expression is NUL-terminated UTF-8. `query` returns **1 on success**, including
an empty result; **0** on invalid arguments, syntax/limit errors or allocation
failure. Optional errors contain an expression byte offset and static message.
`out` must not own a previous result; free it before reuse. Failure zeroes `out`.

Only the result array is allocated/owned. Nodes remain owned by their DOM;
`result_free` does not destroy them, and zeros the result. Keep selected nodes
alive while using them. Selection is a snapshot, not a live query. Neither query
nor result-free mutates the DOM. Large selections use repeated ordered insertion;
this compact evaluator is not an indexed query engine.

## Runnable example

```c
#include <assert.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_XPATH
#include "mulib-xpath.h"

int main(void) {
    const char xml[]="<r><item id='x'>chosen</item><item>other</item></r>";
    mulib_xml_node_t *doc=mulib_xml_parse(xml,sizeof(xml)-1,NULL);
    mulib_xpath_result_t result={NULL,0};
    assert(doc);
    assert(mulib_xpath_query(doc,"//item[@id='x']/text()",&result,NULL));
    assert(result.count==1&&!strcmp(mulib_xml_value(result.nodes[0]),"chosen"));
    mulib_xpath_result_free(&result);
    mulib_xml_destroy(doc);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/xpath.c -o ./mulib-xpath
./mulib-xpath
```

The deeper tests in `tests/test_markup.c` exercise positions, predicate ordering,
CDATA matching, parent/self paths, document order, malformed queries and OOM.
