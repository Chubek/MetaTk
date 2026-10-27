# Working TeX82 source

`TeX.c` is the executable, corrected C translation of Knuth's TeX82 source in
`../TeXSource/TeX.p`, via `../TeXSource/TeX.yaml` and `../Revised/TeX.c`.
Keep the original raw conversion in `../Artifacts/TeX.c` unchanged as a reference.
Build using `../CMakeLists.txt`; there is exactly one `main` in this source.
Do not mix this translation's static global TeX state into a multithreaded
MetaTeX runtime: invoke the standalone compiler as a process instead.
