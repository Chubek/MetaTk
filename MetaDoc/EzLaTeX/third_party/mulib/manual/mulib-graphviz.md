# MuLIB Graphviz DOT output

**Header:** `include/mulib-graphviz.h` · **Language:** ISO C11, C++17 declarations

## Scope and dependencies

Serialize an existing `mulib_graph_t` directed adjacency-matrix graph to quoted
DOT. Every vertex is emitted (including isolated vertices), as are every enabled
directed edge and self-loop. IDs are deterministic `n0`, `n1`, etc. There is no
layout engine, Graphviz library/executable dependency, DOT parser, undirected
conversion or multigraph representation.

The options provide graph/default-node/default-edge attributes and optional
per-vertex labels. They do not provide per-edge/per-node attribute overrides
beyond labels. Names and values are **always quoted strings**: user input is
never treated as raw DOT or Graphviz HTML-like labels. Attribute semantics belong
to Graphviz, not this serializer.

Explicit implementation switches in one TU:

```c
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_GRAPH
#define MULIB_IMPL_GRAPHVIZ
#include "mulib-graphviz.h"
```

The module reuses the existing graph storage and strbuf/UTF-8 implementations;
it does not create another graph representation. Switches are never implicit.

## API and options

```c
typedef struct mulib_graphviz_attr {
    const char *name, *value;
} mulib_graphviz_attr_t;

typedef struct mulib_graphviz_options {
    const char *name;
    const mulib_graphviz_attr_t *graph_attributes; size_t graph_attribute_count;
    const mulib_graphviz_attr_t *node_attributes; size_t node_attribute_count;
    const mulib_graphviz_attr_t *edge_attributes; size_t edge_attribute_count;
    const char *const *labels; size_t label_count;
} mulib_graphviz_options_t;

int mulib_graphviz_quote(mulib_strbuf_t *out, const char *text, size_t length);
char *mulib_graphviz_dot(const mulib_graph_t *graph,
                        const mulib_graphviz_options_t *options, size_t *length);
```

`options` may be NULL; a zero-initialized options object is equivalent. NULL name
defaults to `G`. Attribute arrays are borrowed and required when their count is
nonzero. Attribute names must be nonempty; all names/values must be non-NULL
valid UTF-8. Default attributes are emitted before vertices/edges. Order is the
array order. Duplicate attributes are retained, leaving interpretation to DOT.

Labels apply to vertices `0..label_count-1`; `label_count` must not exceed the
vertex count. NULL label entries omit that label. Quote/backslash characters are
escaped; LF/CR become DOT line-break escape sequences. Tabs remain literal within
quoted strings: DOT has no `\t` label escape, and using one would lose the tab.
Embedded NUL, other ASCII controls and DEL are rejected. Backslashes are literal,
not user-supplied DOT substitutions such as `\N`.

`dot` borrows all input only for the call and returns a malloc-owned,
NUL-terminated string. `length` excludes NUL and is zero on failure. NULL output
means invalid graph/options/text, size overflow or allocation failure; no
`errno` guarantee. The graph must be initialized and not mutated concurrently.
Empty graphs are supported. Runtime is O(V²), matching the existing dense graph.

`quote` appends one complete quoted string to a strbuf and returns 1/0. It
validates before appending, but allocation failure may leave a prefix. Source
must not alias the builder's storage. This helper is for quoted DOT strings, not
HTML labels or arbitrary DOT syntax.

## Runnable example

```c
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_GRAPH
#define MULIB_IMPL_GRAPHVIZ
#include "mulib-graphviz.h"

int main(void) {
    mulib_graph_t graph;
    const char *labels[]={"start","finish"};
    const mulib_graphviz_attr_t attrs[]={ {"shape","box"} };
    mulib_graphviz_options_t options={0};
    char *dot;
    assert(mulib_graph_init(&graph,2)&&mulib_graph_edge(&graph,0,1,1));
    options.labels=labels; options.label_count=2;
    options.node_attributes=attrs; options.node_attribute_count=1;
    dot=mulib_graphviz_dot(&graph,&options,NULL);
    assert(dot&&strstr(dot,"\"n0\" -> \"n1\""));
    free(dot); mulib_graph_free(&graph);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/graphviz.c -o ./mulib-graphviz
./mulib-graphviz
```

The example prints DOT; it can optionally be passed to an independently installed
`dot` program. `tests/test_markup.c` checks quote/injection boundaries, literal
backslashes, labels/default attributes, loops, empty graphs and OOM behavior.
