# Source snapshot and example anchors

All vendor files are byte-identical to the supplied attachments. Source line anchors identify entry points, not a claim that every associated operation is implemented on that line.

| Canonical file | Original upload | SHA-256 |
|---|---|---|
| DSLtk.hpp | DSLtk(3).hpp | `01ce1fe69e1e09f88662946bf9dca19baa7cd7ada9c0ae633d730685f529bfa3` |
| DSLtk.d | DSLtk(1).d | `e11a13d3dde590bd11daffef84ebf5aca9826961e071218a37791fdb1bc3aee8` |
| MetaML.hpp | MetaML(2).hpp | `1477e56be681541983785eedd5c7eb30e8fad02f77f878a90ba390f7d2326b9a` |
| MetaML.d | MetaML.d | `24424b3bde0d152208c6f4d85af1e47f12a3338a2c1096e4c4d363e9f2df0d22` |

| Example | Topic | C++ source entry | Symbol |
|---|---|---|---|
| ex01_01 | A minimal DSL facade | vendor/DSLtk.hpp:5966; vendor/DSLtk.d:93 | `DSL` |
| ex01_02 | Factories without a facade | vendor/DSLtk.hpp:6079; vendor/DSLtk.d:200 | `pipe` |
| ex01_03 | A first MetaML session | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex01_04 | Persistent definitions | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex01_05 | Checking an expression type | vendor/MetaML.hpp:8317; vendor/MetaML.d:6378 | `type_of` |
| ex02_01 | Ordered pipe stages | vendor/DSLtk.hpp:6064; vendor/DSLtk.d:195 | `PipeStage` |
| ex02_02 | Predicate conjunction | vendor/DSLtk.hpp:3086; vendor/DSLtk.d:160 | `Predicate` |
| ex02_03 | Negation and alternatives | vendor/DSLtk.hpp:3086; vendor/DSLtk.d:160 | `Predicate` |
| ex02_04 | Keyed match tables | vendor/DSLtk.hpp:6431; vendor/DSLtk.d:279 | `MatchTable` |
| ex02_05 | Whole-input pattern checks | vendor/DSLtk.hpp:6175; vendor/DSLtk.d:204 | `pattern` |
| ex03_01 | Tagged leaves | vendor/DSLtk.hpp:6561; vendor/DSLtk.d:110 | `ASTNode` |
| ex03_02 | Ordered children | vendor/DSLtk.hpp:6737; vendor/DSLtk.d:107 | `node` |
| ex03_03 | Copying and mutation | vendor/DSLtk.hpp:6561; vendor/DSLtk.d:110 | `ASTNode` |
| ex03_04 | Structural equality | vendor/DSLtk.hpp:6561; vendor/DSLtk.d:110 | `ASTNode` |
| ex03_05 | Debug dumps and interchange | vendor/DSLtk.hpp:6561; vendor/DSLtk.d:110 | `ASTNode` |
| ex04_01 | Optional mapping | vendor/DSLtk.hpp:7529; vendor/DSLtk.d:551 | `Maybe` |
| ex04_02 | Explicit success and failure | vendor/DSLtk.hpp:5425; vendor/DSLtk.d:586 | `Result` |
| ex04_03 | Mapping the success channel | vendor/DSLtk.hpp:5425; vendor/DSLtk.d:586 | `Result` |
| ex04_04 | Deferring one computation | vendor/DSLtk.hpp:7482; vendor/DSLtk.d:534 | `Lazy` |
| ex04_05 | Caching by key | vendor/DSLtk.hpp:7462; vendor/DSLtk.d:531 | `memoize` |
| ex05_01 | One character and end of input | vendor/DSLtk.hpp:8036; vendor/DSLtk.d:827 | `run_parser` |
| ex05_02 | Sequencing two recognizers | vendor/DSLtk.hpp:7850; vendor/DSLtk.d:692 | `Parser` |
| ex05_03 | Explicit backtracking | vendor/DSLtk.hpp:7997; vendor/DSLtk.d:808 | `try_parse` |
| ex05_04 | Repetition and progress | vendor/DSLtk.hpp:8036; vendor/DSLtk.d:827 | `run_parser` |
| ex05_05 | Optional syntax | vendor/DSLtk.hpp:385; vendor/DSLtk.d:708 | `optional` |
| ex06_01 | A grammar with one rule | vendor/DSLtk.hpp:8736; vendor/DSLtk.d:1164 | `create_peg_definition` |
| ex06_02 | Semantic actions and views | vendor/DSLtk.hpp:8229; vendor/DSLtk.d:1017 | `PEGMatch` |
| ex06_03 | Ordered choice is a policy | vendor/DSLtk.hpp:8227; vendor/DSLtk.d:1096 | `PEGDefinition` |
| ex06_04 | Reporting the first unmatched byte | vendor/DSLtk.hpp:8230; vendor/DSLtk.d:1062 | `PEGParseResult` |
| ex06_05 | Preventing zero-width token loops | vendor/DSLtk.hpp:8227; vendor/DSLtk.d:1096 | `PEGDefinition` |
| ex07_01 | Parsing one S-expression | vendor/DSLtk.hpp:9419; vendor/DSLtk.d:2054 | `parse_sexpr` |
| ex07_02 | Quoted atom round trips | vendor/DSLtk.hpp:9236; vendor/DSLtk.d:2026 | `SExpr` |
| ex07_03 | Several top-level forms | vendor/DSLtk.hpp:9406; vendor/DSLtk.d:2466 | `parse_sexpressions` |
| ex07_04 | AST conversion | vendor/DSLtk.hpp:9427; vendor/DSLtk.d:2480 | `ast_to_sexpr` |
| ex07_05 | Validated rule notation | vendor/DSLtk.hpp:9489; vendor/DSLtk.d:2137 | `sexpr_rule` |
| ex08_01 | Graph identity and edges | vendor/DSLtk.hpp:11527; vendor/DSLtk.d:1751 | `DirectedGraph` |
| ex08_02 | Dependency order | vendor/DSLtk.hpp:11527; vendor/DSLtk.d:1751 | `DirectedGraph` |
| ex08_03 | Matching first-order terms | vendor/DSLtk.hpp:9604; vendor/DSLtk.d:2057 | `match_term` |
| ex08_04 | Normalization with an explicit budget | vendor/DSLtk.hpp:11920; vendor/DSLtk.d:2153 | `TermRewriteSystem` |
| ex08_05 | Shared term graphs | vendor/DSLtk.hpp:11983; vendor/DSLtk.d:2216 | `TermGraph` |
| ex09_01 | Adding expressions to an e-graph | vendor/DSLtk.hpp:9551; vendor/DSLtk.d:2542 | `EGraph` |
| ex09_02 | Merging known equivalents | vendor/DSLtk.hpp:9551; vendor/DSLtk.d:2542 | `EGraph` |
| ex09_03 | Saturating a rule set | vendor/DSLtk.hpp:9551; vendor/DSLtk.d:2542 | `EGraph` |
| ex09_04 | Limits are part of the result | vendor/DSLtk.hpp:9525; vendor/DSLtk.d:2534 | `SaturationLimits` |
| ex09_05 | Extraction and cost models | vendor/DSLtk.hpp:9551; vendor/DSLtk.d:2542 | `EGraph` |
| ex10_01 | Static byte regexes | vendor/DSLtk.hpp:10457; vendor/DSLtk.d:2992 | `StaticRegex` |
| ex10_02 | Explicit endianness | vendor/DSLtk.hpp:10543; vendor/DSLtk.d:3040 | `BinaryStream` |
| ex10_03 | Bounded binary reads | vendor/DSLtk.hpp:10543; vendor/DSLtk.d:3040 | `BinaryStream` |
| ex10_04 | Token-level context-free parsing | vendor/DSLtk.hpp:10671; vendor/DSLtk.d:3104 | `CFG` |
| ex10_05 | Epsilon productions | vendor/DSLtk.hpp:10671; vendor/DSLtk.d:3104 | `CFG` |
| ex11_01 | Object macros | vendor/DSLtk.hpp:10931; vendor/DSLtk.d:3245 | `Preprocessor` |
| ex11_02 | Function macros | vendor/DSLtk.hpp:10931; vendor/DSLtk.d:3245 | `Preprocessor` |
| ex11_03 | Half-open spans | vendor/DSLtk.hpp:12144; vendor/DSLtk.d:3954 | `Span` |
| ex11_04 | Mapping offsets to lines | vendor/DSLtk.hpp:12182; vendor/DSLtk.d:3987 | `SourceMap` |
| ex11_05 | Stable diagnostic codes | vendor/DSLtk.hpp:12262; vendor/DSLtk.d:4032 | `Diagnostic` |
| ex12_01 | A schema-derived visitor | vendor/DSLtk.hpp:10149; vendor/DSLtk.d:2855 | `derive_visitor` |
| ex12_02 | Suffix-bearing literals | vendor/DSLtk.hpp:7255; vendor/DSLtk.d:502 | `LiteralSet` |
| ex12_03 | Symbolic enum names | vendor/DSLtk.hpp:5897; vendor/DSLtk.d:78 | `EnumReflection` |
| ex12_04 | Checked visitor boundaries | vendor/DSLtk.hpp:10086; vendor/DSLtk.d:2825 | `ASTVisitor` |
| ex12_05 | Keeping a domain wrapper small | vendor/DSLtk.hpp:7320; vendor/DSLtk.d:515 | `literal_set` |
| ex13_01 | Literals without a prelude | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex13_02 | Evaluation failures are values | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex13_03 | Multi-item source | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex13_04 | Quiet application declarations | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex13_05 | A fresh session for isolation | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex14_01 | Integer arithmetic | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex14_02 | Floating-point rendering | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex14_03 | String primitives operate on bytes | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex14_04 | Tuples are heterogeneous products | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex14_05 | Homogeneous lists | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex15_01 | Anonymous functions | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex15_02 | Curried application | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex15_03 | Local let bindings | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex15_04 | Lexical closures | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex15_05 | Conditional expressions | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex16_01 | Polymorphic identity | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex16_02 | Type errors before evaluation | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex16_03 | Record construction and selection | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex16_04 | Row-polymorphic access | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex16_05 | Functional record updates | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex17_01 | Option without external libraries | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex17_02 | Typed success and error variants | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex17_03 | Declaring a variant | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex17_04 | Pattern guards | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex17_05 | List decomposition | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex18_01 | A module value | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex18_02 | Module functions | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex18_03 | Opening a module | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex18_04 | Nested modules | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex18_05 | Signature-constrained exports | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex19_01 | Registering an integer function | vendor/MetaML.hpp:8460; vendor/MetaML.d:6495 | `register_primitive` |
| ex19_02 | Several native parameters | vendor/MetaML.hpp:8460; vendor/MetaML.d:6495 | `register_primitive` |
| ex19_03 | Boolean decisions | vendor/MetaML.hpp:8460; vendor/MetaML.d:6495 | `register_primitive` |
| ex19_04 | String conversion | vendor/MetaML.hpp:8460; vendor/MetaML.d:6495 | `register_primitive` |
| ex19_05 | Zero-argument native functions | vendor/MetaML.hpp:8460; vendor/MetaML.d:6495 | `register_primitive` |
| ex20_01 | Rendering a language diagnostic | vendor/MetaML.hpp:8437; vendor/MetaML.d:6476 | `render` |
| ex20_02 | Syntax errors versus type errors | vendor/MetaML.hpp:3794; vendor/MetaML.d:2888 | `parse_expression` |
| ex20_03 | Parsing a whole program | vendor/MetaML.hpp:3763; vendor/MetaML.d:2864 | `parse` |
| ex20_04 | Inspecting a function type | vendor/MetaML.hpp:8317; vendor/MetaML.d:6378 | `type_of` |
| ex20_05 | Type queries in a prepared environment | vendor/MetaML.hpp:8317; vendor/MetaML.d:6378 | `type_of` |
| ex21_01 | Replacing the primitive allowlist | vendor/MetaML.hpp:8516; vendor/MetaML.d:6540 | `set_primitive_allowlist` |
| ex21_02 | Capturing output | vendor/MetaML.hpp:8558; vendor/MetaML.d:6566 | `set_output` |
| ex21_03 | Two-phase resource shutdown | vendor/MetaML.hpp:8199; vendor/MetaML.d:6286 | `on_close` |
| ex21_04 | Idempotent close | vendor/MetaML.hpp:8195; vendor/MetaML.d:6293 | `close` |
| ex21_05 | Limiting evaluation depth | vendor/MetaML.hpp:8657; vendor/MetaML.d:6635 | `set_max_depth` |
| ex22_01 | Explicit standard-library discovery | vendor/MetaML.hpp:8250; vendor/MetaML.d:6333 | `load_stdlib` |
| ex22_02 | Import search paths | vendor/MetaML.hpp:8662; vendor/MetaML.d:6636 | `add_search_path` |
| ex22_03 | Unresolved imports are inspectable | vendor/MetaML.hpp:8644; vendor/MetaML.d:6628 | `resolve_import` |
| ex22_04 | Discovering installed primitive names | vendor/MetaML.hpp:8548; vendor/MetaML.d:6563 | `primitive_names` |
| ex22_05 | Portable scripts and native adapters | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex23_01 | Specify and validate the formula input | vendor/DSLtk.hpp:9419; vendor/DSLtk.d:2054 | `parse_sexpr` |
| ex23_02 | Normalize with a small sound rule set | vendor/DSLtk.hpp:11920; vendor/DSLtk.d:2153 | `TermRewriteSystem` |
| ex23_03 | Compare with equality saturation | vendor/DSLtk.hpp:9551; vendor/DSLtk.d:2542 | `EGraph` |
| ex23_04 | Make budget stops observable | vendor/DSLtk.hpp:11920; vendor/DSLtk.d:2153 | `TermRewriteSystem` |
| ex23_05 | Structural regression and semantic regression | vendor/DSLtk.hpp:11920; vendor/DSLtk.d:2153 | `TermRewriteSystem` |
| ex24_01 | A pure policy contract | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
| ex24_02 | Supplying facts through typed callbacks | vendor/MetaML.hpp:8460; vendor/MetaML.d:6495 | `register_primitive` |
| ex24_03 | Validating a reload candidate | vendor/MetaML.hpp:8317; vendor/MetaML.d:6378 | `type_of` |
| ex24_04 | Denying ambient native access | vendor/MetaML.hpp:8516; vendor/MetaML.d:6540 | `set_primitive_allowlist` |
| ex24_05 | Failure reporting and publication discipline | vendor/MetaML.hpp:8171; vendor/MetaML.d:5309 | `Session` |
