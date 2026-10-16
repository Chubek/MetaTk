# 15. Reference tables and migration notes

## Header index

| Header | Main responsibilities |
|---|---|
| `MetaChain.hpp` | collective aliases, outcomes, toolchain, scripts, AST bridge, text commands, version macros |
| `metachain/Common.hpp` | DSLtk include boundary and shared text primitives |
| `metachain/ParseTk.hpp` | registry, grammar declarations, PEG parser, events |
| `metachain/Rewrite.hpp` | terms, substitutions, rules, normalization, events |
| `metachain/MetaVM.hpp` | generic programs, values, VM, traces, JIT protocol |
| `metachain/MetaIL.hpp` | concrete IL, builder, verifier, codecs, optimizer, ISA |
| `metachain/MetaJIT.hpp` | tracing/method/hybrid MetaIL JIT, executable memory, interpreter fallback |
| `metachain/ASMtk.hpp` | x86 encoder, data directives, raw/ELF assembly |
| `metachain/LDtk.hpp` | ELF validation, static linking, bounded file helpers |
| `metachain/Native.hpp` | Linux x86-64 IL-to-assembly lowering |
| `metachain/Text.hpp` | DSLtk-backed term parser, rules, textual grammars |

## Principal result types

| Type | Success condition | Useful data |
|---|---|---|
| `metachain::Outcome<T>` | value exists and no diagnostics | value, diagnostics |
| `parsetk::ParseResult` | `ok` | consumed, error span |
| `rewrite::Result` | inspect `normalized` | term, steps, cycle/limit flags, error |
| `metail::Result` | no diagnostics | program |
| `metail::Execution` | halted and no diagnostics | value, output, steps, status |
| `metajit::Execution` | halted and no execution diagnostics | IL execution fields, statistics, nonfatal compilation diagnostics |
| `asmtk::AssemblyResult` | no diagnostics | bytes, symbols, relocations |
| `ld_tk::Result` | no diagnostics and nonempty image | image, DSLtk report |
| `metachain::NativeSource` | no diagnostics | assembly text |
| `metachain::TextResult` | no diagnostics | text, bytes |

Low-level result types follow their own conventions; in particular,
`rewrite::Result` is not implicitly convertible to bool. Do not mistake a
default-constructed low-level result with no diagnostics for a verified
nonempty program. The collective facade supplies the stricter owned-value
success test for ordinary application use.

## Options quick index

`metachain::Options`:

```text
il          metail::Limits
vm          metavm::VMOptions
assembler   asmtk::Options
linker      ld_tk::Options
rewriting   rewrite::Options
parser      parsetk::Options
text        metachain::text::ParseLimits
optimize    bool (true)
jit         metajit::Options
```

`metavm::VMOptions`: `max_steps`, `collect_trace`, `hot_threshold`,
`max_trace_length`, `max_stack_values`, `max_traces`.

`metajit::Options`: `mode`, `method_threshold`, `max_methods`, `max_code_bytes`,
`max_source_bytes`.

`parsetk::Options`: `skip_whitespace`, `max_depth`, `max_operations`,
`max_events`, `max_input_bytes`, `max_grammar_bytes`.

`rewrite::Options`: `direction`, `max_steps`, compatibility field
`normalize_children_after_rewrite`, `detect_cycles`, `max_depth`, `max_nodes`.

`asmtk::Options`: `little_endian_data`, `allow_unknown_directives`,
`max_output_bytes`, `max_source_bytes`, `origin`.

`ld_tk::Options`: `image_base`, `entry_symbol`, `max_output_size`, compatibility
field `allow_undefined`, `max_input_size`, `max_total_input_size`, `max_objects`.

`metail::Limits`: `max_instructions`, `max_source_bytes`.
`text::ParseLimits`: `max_bytes`, `max_nodes`, `max_depth`.

## Observable compatibility changes from the initial headers

* `MetaIL.hpp` and `MetaChain.hpp` are now implemented, not empty placeholders.
* The assembler defaults to its built-in x86-64 encoder. Optional Symspec
  methods require `METACHAIN_ENABLE_SYMSPEC`; ordinary users no longer acquire
  a Boost.JSON dependency merely by including the assembler.
* Unsupported instructions/directives still fail, but supported machine forms
  now encode real instructions and can emit interoperable ELF objects.
* Integer data operands are range-checked instead of silently truncating.
  Comments/operand splitting honor quotes, escapes are strict, and NUL bytes
  count toward string output limits.
* ParseTk adds ordered choice, grouping, repetition, lookahead, progress-aware
  recursion detection, cached regexes, bounds, and committed event delivery.
  Listeners no longer observe failed speculative branches.
* Rewriting counts actual replacements rather than arbitrary whole-tree sweeps.
  Identity rewrites are no-ops, unbound RHS variables fail early, failed matches
  do not contaminate substitutions, and cycles use structural keys.
* MetaVM distinguishes an explicit self-jump from an unchanged PC, exposes a
  const loaded program, records actual loop paths rather than only the backedge
  source, and can have its trace subscriptions cleared. Load again instead of
  mutating live code behind traces.
* The facade gained `link_files`, `parse_term`, `parse_rules`, a text
  `executable` overload, and `native_stage`/`build_stage` pipelines.
  `metail::run` accepts explicit verification limits so the facade configuration
  bounds every stage.
* LDtk enforces stricter ELF invariants, checked relocations, input/output
  bounds, weak resolution, executable entry validation, and W^X layout. Inputs
  previously accepted while unsupported features were ignored may now fail.

These changes intentionally favor explicit correctness over accidentally
preserving incomplete behavior. Re-run application tests when upgrading, and
do not treat the original starter interfaces as a stable binary ABI.

## Troubleshooting

**Cannot find DSLtk.hpp.** Set the DSLtk include directory or
`METACHAIN_DSLTK_HEADER`. Keep macros consistent across translation units.

**Unexpected trailing input.** Check PEG ordered choice and EOF requirements.
Put longer overlapping alternatives first, or factor the common prefix.

**Left recursion detected.** Replace left recursion with repetition/right
recursion. The parser does not seed-grow left-recursive PEG productions.

**Repetition matched empty input.** Ensure each repeated expression consumes
source. An optional/empty expression inside `*` cannot make progress.

**Unbound rewrite RHS variable.** Every RHS variable must be matched on the LHS.
Use a quoted atom for a literal string starting with `$`.

**Instruction or operand unsupported.** Consult chapter 6's exact forms.
Loading metadata is not an encoder extension. Use explicit widths/registers
and the supported `[base+disp]` or `[rel symbol]` address forms.

**Undefined object symbol requires .extern.** Declare intended cross-object
references and provide a strong or weak definition when linking. Raw output
cannot carry unresolved references.

**Unsupported allocated section flags/relocation.** The object producer emitted
a feature outside LDtk's contract. Disable that feature deliberately or use a
linker that implements it; do not strip flags until the underlying semantics
have been understood.

**Native executable returns a surprising code.** Unix exposes only the low
eight result bits. Native runtime errors use 124–127; interpreted results and
errors remain structured and distinguishable.

**A diagnostic is reported with stage `IO`.** It came from `link_files` reading
an input. Check the path, permissions, and configured per-input/aggregate limits.
Other link failures use stage `Link`.

**Build succeeds but the file does not run.** Confirm Linux x86-64 target,
execute permission, filesystem mount policy, image address constraints, and
host security/resource settings. Byte generation is not execution permission.
