# Chapter 26 — Output Backends

**Status:** scaffold contracts (`stdbe/MVBE-*.cpp` + `*.csel`). No backend
renders anything today.

## The eleven targets

| Backend | Format / library |
| --- | --- |
| PostScript | `.ps` / EPS output |
| TikZ | PGF/TikZ for LaTeX |
| PSTricks | PSTricks for LaTeX |
| Asymptote | Asymptote language |
| Cairo | Cairo 2D library |
| Skia | Skia graphics engine |
| NanoVG | NanoVG immediate-mode library |
| Clipper2 | Clipper2 polygon engine |
| LaTeXPicture | LaTeX `picture` environment |
| TROFF | `troff`/`groff` picture requests |

Each pairs a `.cpp` emitter stub with its `.csel` selector rules.

## Backend contract (intended)

Emitters consume validated, rewritten MVIR and produce bytes; they never
reparse source, never resolve names, and never change program meaning for
presentation convenience. Numeric output must preserve the
floating-point behavior in force (chapter 23's corner-case rule applies at
emission too). Backend-specific behavior belongs here and under
`include/` — never in the frontend.
