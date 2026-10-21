# XBind examples

- `hello/` is the fixture used by the spec regimen and executable Lua generator tests. It describes a
  tiny C library with a Lua `.xbind` spec and enough annotations to exercise all
  nine backend targets, functions, params, and type overrides.

The fixture intentionally uses only default-mapped scalar/string types (`int`,
`double`, `const char *`) so it stays useful while the real C parser and target
emitters are still being implemented.
