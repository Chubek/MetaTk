# hello XBind fixture

This fixture is intentionally small. It gives the driver one C header and one
`.xbind` spec to resolve before the real parser/generator exists.

Expected spec probes (the fixture is also compiled/executed by `xbind.codegen`):

```sh
xbind --find-spec hello examples/hello
xbind --dump-spec hello examples/hello
xbind --target Lua
```

The spec targets every supported backend: D, Go, Java, Lua, NodeJS, OCaml,
Python, Ruby, and Rust.

Future end-to-end checks should generate and run bindings for `hello_add`,
`hello_scale`, and `hello_greet`.
