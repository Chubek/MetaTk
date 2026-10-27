# Compatibility matrix (tested feature subset)

| Feature | Current behavior | Limitations |
|---|---|---|
| `\def`, `\gdef`, `\let` | undelimited/delimited parameters, literal prefixes, brace delimiters | not all primitive-meaning aliases or long/outer semantics |
| `\edef`, `\xdef` | shared expansion scanner | native expandable subset |
| Group scoping | macros, handlers, environments, registers, catcodes and styles; `\global` assignments | not full TeX group-local state |
| TeX catcodes | incremental scoped/global assignments for codes 0–255 | no general Unicode catcode table or `\endlinechar` |
| Conditionals | `iftrue/iffalse/ifdefined/ifnum/ifdim` | not general TeX conditional grammar |
| `\input{path}` | nested relative paths and file diagnostics, depth limit 16 | local I/O, no root sandbox; token execution frames do not span input boundaries |
| `$...$`, `$$...$$`, `\(...\)`, `\[...\]` | implemented | MathML subset; no OpenType MATH layout |
| `\newcommand`/`\renewcommand` | optional first default, validation, stars, scope | no long/short distinction or robust/xparse machinery |
| `\newenvironment`, `\renewenvironment` | scoped parameters/defaults, captured end handlers | parameters only in begin body |
| `\newtheorem`, `proof` | shared/parent-relative numbering, optional titles, unnumbered forms | semantic quote blocks, not theorem-style layout |
| Headings, labels, references | implemented | simplified numbering; `pageref` not accurate |
| itemize/enumerate/description | per-list ordinals and indentation, bold item labels | flattening/nesting approximate |
| `\includegraphics` | HTML `<img>` | fallback PDF draws placeholder |
| Verbatim code | literal `\verb`, `verbatim`, NekoTeX code arguments/environments/files | direct-source environments; stored macro tokens already underwent scanning |
| LaTeX packages | recognized subset of names | `.sty` files are not loaded |
| NekoTeX | typed multi-paragraph notes, numbered exercises, literal code and language metadata | semantic extensions; no syntax highlighter |
| MetaVect | inline/file/environment scenes, scoped reusable scene commands, physical-unit fitting/scaling; native vectors | scene v1 artifacts; no `.mvct` evaluation |
| REPL | replxx editing, completion, history, persistent definitions | incremental block output; not a full interactive TeX engine |
| MetaML commands | `\mmlcommand`, `\mmlcommandfile`; all implemented metalib modules | inline bodies use TeX token rules; files preserve arbitrary MetaML source |
| External commands | `\Extern`, `\externcommand`; shebang and descriptor capture | POSIX executable scripts, bounded output and execution time |
| Counter arithmetic | strict integers and checked add/step | C++ `int` range; not TeX registers |
| Unicode text | preserved in HTML and JSON | proper Unicode PDF requires PangoCairo |
| Font selection/kerning | PangoCairo if installed | fallback PDF has approximate metrics |
| DVI | TeXScrape compiler and DVI validator | separate from native semantic engine |
| PostScript/EPUB/troff IR | native Document-based outputs | documented subset devices |
| Lua | native binding | advanced MIDL/TeXpresso modules remain scaffolds |

The engine is designed to be extended, but there is no basis for claiming
LaTeX or plain TeX conformance against TUG's official test suites yet.
See [plugin contracts](PLUGINS.md) for exact syntax and verification targets.
