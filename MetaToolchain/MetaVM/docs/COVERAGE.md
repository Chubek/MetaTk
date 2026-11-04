# Machine coverage

Portable examples run on Machinery. Native support is separate. DVI has a standard format-2 loader; no complete language/platform runtime is provided.

8 executable native subsets; 41 catalog-only profiles. SECD and Krivine use explicit in-memory closure-machine profiles; see [CLOSURE_VMS.md](CLOSURE_VMS.md).

DVI version 2 and VMWeave event replay are specified in [DVI.md](DVI.md). The manual for every legacy entry is [VMS.md](VMS.md). The SSAVM i64 textual subset and its configurable limits are specified in [SSAVM.md](SSAVM.md). The validated MetaML Definition embedding is described in [ROUND3.md](ROUND3.md). CHIP-8 presets and pinned ROM coverage are specified in [CHIP8.md](CHIP8.md); SDK packaging is described in [PACKAGING.md](PACKAGING.md).

| Machine | Architecture | Native coverage |
|---|---|---|
| ALEX | unspecified | catalog-only |
| ART | runtime | catalog-only |
| AVM2 | stack | catalog-only |
| B-Code | unspecified | catalog-only |
| BCPL | stack | catalog-only |
| BEAM | register | catalog-only |
| C-Machine | unspecified | catalog-only |
| CAM | closure | catalog-only |
| CESK | closure | catalog-only |
| CHIP-8 | register | classic-opcode-interpreter |
| CLR | stack | catalog-only |
| CPython | stack | catalog-only |
| ConcurrentML | runtime | catalog-only |
| DRMVM | graphics | graphics-subset |
| DVI | document | dvi2-interpreter |
| Dalvik | register | catalog-only |
| Dart | runtime | catalog-only |
| Dis | register | catalog-only |
| FAM | unspecified | catalog-only |
| FORTH | stack | integer-word-subset |
| FUL | unspecified | catalog-only |
| G-Code | motion | catalog-only |
| Harrison | unspecified | catalog-only |
| IKL | unspecified | catalog-only |
| JSCore | runtime | catalog-only |
| JVM | stack | integer-method-subset |
| Krivine | closure | krivine-whnf-subset |
| LuaVM | register | catalog-only |
| Lucide | unspecified | catalog-only |
| Monty | unspecified | catalog-only |
| MoscowML | closure | catalog-only |
| P-Code | stack | catalog-only |
| P4 | unspecified | catalog-only |
| ParrotVM | register | catalog-only |
| PostScript | stack | catalog-only |
| Prol | unspecified | catalog-only |
| SCUMM | script | catalog-only |
| SECD | closure | secd-unary-subset |
| SOM | object | catalog-only |
| SSAVM | ssa | issa-subset |
| Self | object | catalog-only |
| Smaltalk-80 | object | catalog-only |
| TAMMY | unspecified | catalog-only |
| V8 | runtime | catalog-only |
| WAM | logic | catalog-only |
| WASM | stack | catalog-only |
| YARV | stack | catalog-only |
| Z-Machine | unspecified | catalog-only |
| Zand | unspecified | catalog-only |
