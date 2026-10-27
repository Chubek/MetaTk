# 39 — MetaML commands

MetaML-backed commands are installed by the native engine. Define one with
`\mmlcommand{\NAME}[ARITY]{BODY}`, or read its body from a file with
`\mmlcommandfile{\NAME}[ARITY]{PATH}`. Arity defaults to zero and is limited
to nine required arguments.

<!-- metatex-example {"format":"plain","text":"Hello, WORLD!"} -->
```tex
\mmlcommand{\greet}[1]{"Hello, " ^ String.upper arg1 ^ "!"}
\greet{world}
```

Arguments become MetaML strings `arg1` through `arg9`, and are also available
through the MetaML process-argument interface. Each invocation creates a fresh
session, installs the implemented host bundle, loads the prelude, and evaluates
the body. Captured print output and a final string result become generated
TeX input. A non-string non-Unit result uses its textual representation.

Definitions obey group scope. File bodies are read at definition time, and
imports resolve relative to the defining source. Prefer file bodies when
MetaML needs significant whitespace or characters such as `%`, which normal
TeX argument scanning would interpret.

Generated source is currently tokenized with default categories before
insertion. It can define and invoke macros, but does not provide a fully
incremental category-aware generated-source frame. Output is limited to 4 MiB
and evaluation has a deadline; native host calls retain their own blocking
behavior. See [the scripting guide](../docs/SCRIPTING.md) for library search
paths, available host modules, and runtime limits.

[Contents](README.md)
