# mulib-ast

Define `MULIB_IMPL_AST` and `MULIB_IMPL_STRBUF` in one translation unit.

`mulib_ast_create(production,text,length)` copies its arguments; text is a byte
range and may contain NUL. `mulib_ast_append(parent,child)` transfers ownership
on success and rejects already attached nodes, cycles and depth above 256.
Destroy roots with `mulib_ast_destroy`. Do not edit linkage fields directly,
attach siblings manually, destroy attached nodes, or mutate a traversed tree.

Each node has a production, matched text, source offset, optional action alias,
and ordered children. `mulib_ast_set_action` copies an alias. Set offsets directly
when building trees manually. Trees returned by the grammar loader are entirely
independent of both grammar and source lifetimes.

`mulib_ast_render(root,format)` returns an allocated string; use `free` on it.

* `MULIB_AST_SEXPR`: `("production" "text" child...)`, with JSON-escaped strings.
* `MULIB_AST_JSON`: objects with `production`, `text`, `offset`, `length`, `children`.
* `MULIB_AST_YAML`: `---` followed by JSON flow notation, valid YAML 1.2.

Valid UTF-8 is preserved. Control bytes and invalid high bytes are represented
as `\u00XX`; invalid UTF-8 is thus mapped to Latin-1 codepoints, not silently
emitted as invalid JSON. Binary byte identity requires consulting `length` and
the original tree. S-expression escapes are richer than the existing minimal
`mulib-sexpr` reader; no round-trip through that reader is promised for control
bytes. Serialization is a structural representation, not source-code generation.

`mulib_ast_walk(root,actions,count)` dispatches an array of `mulib_ast_walk_t`
in postorder. An entry matches its production name, an explicitly assigned
action alias, or all nodes if its production is NULL. Each matching entry runs
once per node. Nonzero callbacks continue; zero stops. Return values are 1 for
completion, 0 for a callback stop, -1 for invalid arguments. Callbacks may inspect
nodes and update user context, but must not mutate or destroy the tree.

A `mulib_ast_walker_t` owns its root. Use `mulib_ast_walker_walk` and
`mulib_ast_walker_destroy` for walkers returned by the parser.

See also [visitors](mulib-visitor.md), [listeners](mulib-listener.md),
[event streams](mulib-evtstream.md), and [grammar loading](mulib-parsec.md#owned-grammar-loading).
