# The Juniper manual

**36 chapters, from your first macro to a checked backend pipeline.** This manual
describes the implementation shipped in this repository, not a hypothetical future
compiler. Its examples use real GPPX expansion, MetaIL passes, Juniper selection
and scheduling, and the optional MetaArch form encoder.

## Before you start

The source directory is **`Juniper/`**, with a capital `J`. The executable is
`juniper`, the C++ namespace is `juniper`, and headers are under `juniper/`.
Shell commands assume a POSIX shell and start in the MetaTk workspace root unless
a command explicitly changes directory. Chapter 2 establishes these variables:

```sh
export JUNIPER_ROOT="$PWD/Juniper"
export J="$JUNIPER_ROOT/build/juniper"
export METAIL_HOME="$PWD/MetaToolchain/MetaIL"
export MANUAL_EXAMPLES="$JUNIPER_ROOT/manual/examples"
export WORK="$PWD/juniper-work"
mkdir -p "$WORK"
```

`WORK` is for your generated files, not a directory of installed Juniper files.
Adjust it before copying examples if that directory already contains your work.
Do not replace the source `Juniperfile` with its generated output.

Complete Juniper programs use `juniper` code fences. Fragments, intentionally
incomplete grammar, and explanatory pseudo-assembly are labeled as such. An
example that is intentionally rejected says so explicitly. A few standalone C++
examples include the supplied [Support.hpp](examples/Support.hpp); this is an
application helper, **not** a hidden part of Juniper's public API.

## Contents

### Part I — Get productive

1. [The compilation model and its boundaries](01-compilation-model.md)
2. [Building Juniper and setting up a workspace](02-building.md)
3. [Installing and locating runtime resources](03-installation-and-runtime.md)
4. [Your first Juniperfile](04-first-juniperfile.md)
5. [The command-line workflow](05-command-line.md)
6. [Source syntax and the two language layers](06-source-syntax.md)
7. [Defining and expanding macros](07-macro-basics.md)
8. [Macro parameters, grouping, and hygiene](08-macro-parameters.md)
9. [Includes and project organization](09-includes.md)
10. [Conditional compilation and configuration](10-configuration.md)

### Part II — Write correct IR

11. [Modules, functions, parameters, and calls](11-functions-and-calls.md)
12. [Integers, operators, and overflow](12-integer-semantics.md)
13. [Mutable variables and statements](13-variables-and-statements.md)
14. [Labels, branches, and loops](14-control-flow.md)
15. [Lazy select and eager logical operations](15-lazy-select.md)
16. [Effects, traps, and assumptions](16-effects-and-traps.md)
17. [Canonical IR and reproducible round trips](17-canonical-ir.md)
18. [Using the real MetaIL optimizer](18-optimization.md)
19. [Analysis and inspecting program structure](19-analysis.md)

### Part III — Embed Juniper in C++

20. [Compiler contexts, results, and ownership](20-cpp-contexts.md)
21. [Constructing native IR values](21-native-ir.md)
22. [Editing and traversing owned modules](22-editing-ir.md)
23. [Embedding GPPX preprocessing](23-preprocessing-api.md)

### Part IV — Build a backend

24. [Backend profiles and inheritance](24-backend-profiles.md)
25. [Tree patterns and selection costs](25-selection-costs.md)
26. [Immediates, coverage, and legalization boundaries](26-immediates-and-coverage.md)
27. [Fused instructions without changing semantics](27-fusion.md)
28. [Machine IR and its JSON inspection format](28-machine-ir.md)
29. [Scheduling, latency, and resource capacities](29-scheduling.md)
30. [Mutable-register hazards and full fences](30-hazards-and-fences.md)
31. [An end-to-end C++ backend pipeline](31-cpp-pipeline.md)
32. [Checked instruction encoding with MetaArch](32-metaarch-encoding.md)

### Part V — Ship and maintain a project

33. [CMake integration and dependency tracking](33-cmake.md)
34. [Diagnostics, limits, and secure operation](34-diagnostics-and-security.md)
35. [Testing and reproducible backend development](35-testing.md)
36. [Capstone: a configurable affine backend](36-capstone.md)

## Reading routes

* **CLI user:** 1–19, then 33–36.
* **Embedding author:** 1–6, 11–23, 28, 31, 33–35.
* **Backend author:** integer/effect semantics in 12–16, then 24–36.
* **In a hurry:** 2, 4, 5, 17, 24, 29, and 33.

The distinction to remember throughout: **`emit` produces round-trippable source
IR; `select` and `schedule` produce machine-IR inspection JSON.** JSON is not an
input language. Juniper does not provide a `run`, `jit`, `allocate`, or `link`
command, and selecting an opcode does not implement that opcode in hardware.

## Run the manual's examples

After building Juniper:

```sh
python3 "$JUNIPER_ROOT/manual/check_examples.py" \
  --juniper "$J" --metail-home "$METAIL_HOME"
```

To additionally compile and execute the marked standalone C++ examples, including
the optional synthetic MetaArch encoding example:

```sh
python3 "$JUNIPER_ROOT/manual/check_examples.py" \
  --juniper "$J" --metail-home "$METAIL_HOME" \
  --cpp --with-metaarch --build-dir "$WORK/manual-check"
```

The checker also verifies the 36-chapter sequence and local Markdown links. It
validates marked Python snippets syntactically; programs that inspect generated
artifacts are demonstrated with their required input files in the chapters.
The [example directory](examples/README.md) contains the multi-file projects.

See the shorter [language reference](../docs/Language.md),
[backend contracts](../docs/Backend.md), and
[implementation verification record](../docs/Verification.md) for supporting
reference material.
