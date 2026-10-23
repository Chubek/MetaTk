# 31. IO and observable effects

IO supplies string output, line input, and inspection helpers. The prelude's `print` and `println` both print a line; use `IO.print` when no newline is desired. `show_line` renders an arbitrary value with Core's notation before printing.

```metaml
IO.print "answer="
IO.println (Int.to_string 42)
show_line (Some [1, 2, 3])
Test.equal "IO returns unit" (IO.print "") ()
```

`IO.read_line ()` returns `Option String`; end of input yields None. The ordinary line terminator is removed. `IO.eprint` writes to the process's error stream. The optional host IO module adds formatting, output flushing, multi-line output, and additional stream helpers.

Embeddings can redirect ordinary print output with `Session.set_output`, receiving byte strings rather than already-formatted diagnostic objects. The C adapter captures that output in an internal buffer retrievable with `metaml_output`. Error-stream output and input still use process streams unless an application replaces or avoids those primitives.

Output callbacks should be fast, exception-safe, and non-reentrant. The session serializes output with a mutex; recursively invoking the same session or attempting another locked output operation from the callback can deadlock. A queue into the application's event loop is often a better integration than synchronous UI work inside the callback.

IO is eager. `map` over a printing callback produces effects as it traverses; a lazy-looking closure delays only its body until called. Prefer `List.for_each` when the operation's purpose is effects rather than building a list of units.

The primitive allowlist is not a complete effect system. Core IO is part of the fresh runtime, imports can read source files, and `Frontend.eval` can execute core effects in its nested session. If all external access must be forbidden, isolate execution at the process level and restrict the runtime surface deliberately.

**Sources:** [`io/mod.ml`](../metalib/io/mod.ml), [`cpp/IO.h`](../metalib/cpp/IO.h).

<!-- nav:begin -->
[Contents](README.md) · [Previous](30-result-library.md) · [Next](32-buffers.md)
<!-- nav:end -->
