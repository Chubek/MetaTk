# 22. Source imports and library discovery

`import "spec"` loads a source unit into the session. Resolution tries the specification as a file, with `.ml` appended, and as a directory containing `mod.ml`. Search bases include the importing file's directory, the working directory, embedding search paths, and the selected standard-library directory.

```metaml
import "list"
Test.equal "imported API" (List.map (+ 1) [1, 2]) [2, 3]
```

Successful imports are tracked by canonical path and normally execute only once. A failed import is removed from that success cache, allowing corrected source to be imported again. This does not roll back earlier successful definitions or external effects from the failing source; retryable means the load is attempted again, not that the whole session is transactional.

The interpreter inserts a path before evaluating its contents to prevent unbounded recursive import loading. Cyclic imports are therefore not equivalent to mutually recursive module declarations with fully available members. Organize shared interfaces so initialization order is explicit.

`load_stdlib` searches `$METAML_STDLIB`, a dialect-specific path under `$METAML_PREFIX`, paths relative to the executable, an optional compile-time default, and a working-directory `metalib` fallback. Pass an explicit directory for reproducible embedded deployments. The directory must contain `prelude/mod.ml`.

```sh
METAML_STDLIB=/opt/myapp/metalib build/metaml-repl-cpp program.ml
```

The prelude imports the portable modules and opens Core. Fresh embedding sessions do not silently locate and execute external library files; call the load operation and check its result. The supplied REPL performs this step unless disabled.

`#load` and `#reload` deliberately execute a selected file rather than behaving exactly like a once-only import. Use them for interactive editing; use imports for stable dependency loading. Missing imports use `M0505`, while file-loading directives can report `M0603`.

<!-- nav:begin -->
[Contents](README.md) · [Previous](21-functors.md) · [Next](23-diagnostics.md)
<!-- nav:end -->
