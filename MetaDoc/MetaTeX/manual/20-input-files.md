# 20 — Multi-file documents

Use a braced filename with `\input`:

```tex
\input{chapters/introduction}
\input{chapters/results.tex}
```

If the path has no extension, `.tex` is appended. Relative paths resolve from
the file containing the command; nested inputs therefore remain portable when
the whole document tree is moved. Give the embedding API an actual filename
when it should resolve includes relative to that source location.

Included source uses the current category table and executes with current
definitions and registers. An input file does not automatically introduce a
group. Wrap the `\input` call in braces if its local definitions should be
discarded afterward. New category assignments within the file affect its
remaining unread source and, when not local, following caller source.

`\endinput` stops the current input frame. `\bye` stops document execution.
The native filename scanner uses an argument interface; use braces rather
than assuming all of TeX's unbraced filename scanning conventions.

Input nesting is limited to 16 and ordinary source reads to 16 MiB. Errors in
included files carry that file's path and source location where available.
An unsuccessful include can leave earlier assignments in place; includes are
not transactional modules. Package loading through `.sty` or `.cls` files is
a separate, currently unimplemented facility.

[Contents](README.md)
