# Compatibility matrix (tested feature subset)

| Feature | Current behavior | Limitations |
|---|---|---|
| `\def`, `\gdef`, `\let` | implemented | undelimited `#1`..`#9` only |
| `\edef`, `\xdef` | accepted | behave as unexpanded definitions |
| Group scoping | macros and font styles | no `\global` prefix primitive; not full group-local state |
| TeX catcodes | fixed lexer behavior | `\catcode` mutation not implemented |
| Conditionals | `iftrue/iffalse/ifdefined/ifnum` | not general TeX conditional grammar |
| `\input{path}` | nested relative paths and file diagnostics, depth limit 16 | local I/O, no root sandbox; token execution frames do not span input boundaries |
| `$...$`, `$$...$$`, `\(...\)`, `\[...\]` | implemented | MathML subset; no OpenType MATH layout |
| `\newcommand`/`\renewcommand` | implemented | no optional default parameter |
| `\newenvironment`, `\newtheorem` | supported | zero-argument environments |
| Headings, labels, references | implemented | simplified numbering; `pageref` not accurate |
| itemize/enumerate | implemented | flattening/nesting approximate |
| `\includegraphics` | HTML `<img>` | fallback PDF draws placeholder |
| Verbatim code | limited `\verb` | lexer still interprets `%` comments |
| LaTeX packages | recognized subset of names | `.sty` files are not loaded |
| NekoTeX | independent dynamically loaded plugin | current semantic extensions only |
| MetaVect | `\metavect`, `\metavectfile`; SVG/HTML and native PDF vectors | scene v1 artifacts; no `.mvct` evaluation |
| REPL | replxx editing, completion, history, persistent definitions | incremental block output; not a full interactive TeX engine |
| MetaML commands | `\mmlcommand`, `\mmlcommandfile`; all implemented metalib modules | inline bodies use TeX token rules; files preserve arbitrary MetaML source |
| External commands | `\Extern`, `\externcommand`; shebang and descriptor capture | POSIX executable scripts, bounded output and execution time |
| Counter arithmetic | strict integers and checked add/step | C++ `int` range; not TeX registers |
| Unicode text | preserved in HTML and JSON | proper Unicode PDF requires PangoCairo |
| Font selection/kerning | PangoCairo if installed | fallback PDF has approximate metrics |
| DVI/PostScript/EPUB | not implemented | pre-existing empty files retained |
| Lua and external IR files | not implemented | supplied files were empty |

The engine is designed to be extended, but there is no basis for claiming
LaTeX or plain TeX conformance against TUG's official test suites yet.
