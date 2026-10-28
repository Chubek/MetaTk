# Verification report

Generated after `make verify`, 2026-10-22 08:40 UTC.

## Book

- Exactly **42 numbered chapters**, including five case studies.
- **466 PDF pages**, 42 figures, 70 table environments, and
  54 pseudocode blocks.
- 164,837 source words, including code
  and TeX markup; each chapter passes the 2,500-source-word minimum check.
- Chapter ordering, graph/table/pseudocode presence, listing assets, label
  uniqueness, and reference targets checked by `scripts/check_book.py`.
- LuaLaTeX/BibTeX build completed. Final log checked for undefined references,
  duplicate labels, missing glyphs, LaTeX errors, and overfull boxes.
- 42 Markdown chapter exports plus a combined Markdown volume generated.

## Executable evidence

- Five C++20 case-study programs built and ran with assertions enabled.
- 47 complete C++ chapter programs compiled and exited successfully,
  including the optional Lua examples. Source excerpts, fragments, and negative
  examples are inventoried separately; they are not counted as standalone programs.
- The D importing companion compiled and ran with LDC.
- Four header probes passed their expected outcomes: structural-object pattern
  dispatch compiled and ran; closure predicate composition, duplicate-payload
  Result inspection, and the C++ task runner were rejected as documented.

Case-study output:

```text
1500 m / 120 s = 12.5 m/s
query: Ada, Grace (original and optimized results agree)
port=9000 mode=debug
(mul (add a 0) 1) => a
saturated=1 iterations=2 applications=4
frame=sensor_7 bytes=14 partial-I/O round trip OK
```

D companion output:

```text
10 (add (number 1) (number 2)) a
```

Per-program source, compiler logs, exit status, and captured output are under
`build/verification/`. The JSON listing inventory records which listings are
fragments. Passing compile/run checks do not constitute a proof of every
general statement in the text, a performance benchmark, or exhaustive input
coverage. The five case studies add domain assertions for their stated contracts.

## Implementation fingerprints

| Supplied file | SHA-256 |
|---|---|
| `DSLtk/DSLtk.hpp` | `b81ceba45efb53c55c8525e29b7ab942e5797cc50ca388d138d6b386c9c0bdca` |
| `DSLtk/DSLtk.d` | `c9a7ab135c02edf644144d2d4cee98291a92f3dbb4696ab3d84bd0100dc51600` |

## Toolchain

```text
g++ (GCC) 16.2.1 20260810
LDC - the LLVM D compiler (1.43.0):
pandoc 3.10.2
This is LuaHBTeX, Version 1.18.0 (TeX Live 2024)
BibTeX 0.99d (TeX Live 2024)
```

The default bibliography backend is BibTeX, which is usable with the installed
TeX Live. The installed Biber executable required an unavailable `libcrypt.so.1`;
the book therefore uses biblatex's supported BibTeX backend for its six entries.

## Reproduce

```sh
make -C DSLtkBook verify
```

Successful listing/probe results are reused only when their extracted source,
compiler identity, flags, and header digest match. Remove `build/verification/`
to force those checks again. See `README.md` for dependencies and individual targets.
