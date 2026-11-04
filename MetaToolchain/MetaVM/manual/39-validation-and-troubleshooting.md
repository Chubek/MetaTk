# Chapter 39 — Validation, troubleshooting, and measurement

[Previous](38-installation-and-sdk.md) · [Contents](README.md) · [Next](40-compatibility-and-roadmap.md)

## Reproducible validation ladder

1. Run `python3 tools/generate_catalog.py --check` and
   `python3 tools/validate_catalog.py`.
2. Build/run the fallback suite with `make test` and `make sanitize`.
3. Build separate GCC and Clang Release trees, keeping tests active under NDEBUG.
4. Run a Debug ASan/UBSan CMake tree and record leak-detection policy.
5. Exercise real optional integrations only with matching dependencies.
6. Run shared-library/relocated SDK tests and independent native oracles.

```sh
ctest --test-dir out-manual --output-on-failure
ctest --test-dir out-manual -R 'metavm_(catalog|definition)' --output-on-failure
python3 tools/test_headers.py cc c++
python3 tools/generate_vmspec_manual.py --check
```

With optional MetaML, the header test accepts a third argument naming matching
DSLtk's include directory. Direct script names and executable paths differ
between the fallback and CMake builds; the registered CTest commands select
the intended paths.

## Coverage evidence, not a single pass count

Core tests retain 5,056 checks including deterministic binary mutations and
random assembly inputs. Framework checks add 49 factory examples, all emitted
C factories/binary parity, both engines, Definition arithmetic/state lowering,
transaction rollback, native traps/budgets, SSA PHIs/analysis, closure
differentials, DVI mutations/replay, and pinned CHIP-8 ROMs.

The recorded generation-repair configurations passed GCC Release 22/22,
Clang Release with real MetaML 24/24, Clang Debug ASan/UBSan without MetaML
22/22, and GCC shared/relocated `lib64` SDK 22/22. GCC fallback normal/sanitized
suites also passed. These are dated observations, not guaranteed counts for
every option combination or future revision. See `docs/VALIDATION.md` for history.

When TeX and DVItype are present, DVI CLI tests generate actual documents and
compare rule/glyph positions and font advances against DVItype. Otherwise that
independent check is explicitly skipped. A skip must not be reported as oracle
validation. MetaTeX/libdrm and non-Linux validation remain separate gaps.

## Troubleshooting reference

| Symptom | Check first |
|---|---|
| CMake cannot find `vmweave` sources | Required tracked sources missing/deleted |
| Missing generated file or stale metadata | Generator `--check`; regenerate owned outputs |
| No `metavm-run` | `METAVM_BUILD_EXAMPLES` and `Machinery/` output location |
| Missing host import | Exact name/arity; register all imports before run |
| Integer loop exhausts fuel | Zero is truthy in Machinery; produce boolean comparison |
| Object disappears or foreign-value error | Same-VM ownership and pinning across allocations |
| MetaML missing types | Wrong DSLtk revision/include directory, not a stub opportunity |
| Definition rejects branch | Absolute byte address, alignment, static resolvability |
| SSAVM parse succeeds but execution traps | Structural checking is not analysis/runtime proof |
| Installed consumer cannot find package | Prefix path, custom libdir, co-installed Machinery config |
| Sanitizer leak report only with MetaML | Separate documented type-checker retained allocations |
| VIP draw/key instruction stays blocked | Continue host ticks and sampled transitions |

## Measure the right layer

Separate parsing, lowering, serialization, dispatch preparation, execution,
host work, and result copying. Record module/program size, limits, profile,
engine, compiler/options, host schedule, and warm/cold cache policy. Machinery
steps, Definition lowered steps, closure transitions, and wall-clock time are
different quantities. VM heap bytes are not RSS. Concurrent CI timings are
smoke-test observations, not stable performance budgets.

Never disable checks, replace an unknown instruction, or raise every limit
until a problematic input appears to work. Minimize the input, retain its
diagnostic, add a regression, and document what the fix actually establishes.
