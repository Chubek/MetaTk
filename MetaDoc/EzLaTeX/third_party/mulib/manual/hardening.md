# Header hardening and verification

This pass checks the complete `include/mulib-*.h` inventory, not just newly added
modules. Compilation, static analysis and finite tests are **not** a certification
of memory safety, complete standards support, or readiness for hostile production
traffic. Files without a demonstrated defect need not receive cosmetic edits.

## Coverage across all headers

`tools/check.py` checks every header against `MANIFEST.yaml`, compiles its C11
implementation independently with transitive dependencies, checks standalone
declarations in C11 and C++17, and links all implementations together. Every
module's standalone example is built and run. Existing focused suites exercise
containers, indexes, allocators, parser/grammar/AST machinery, markup, configuration,
protocols, storage, runtimes, terminals, network addresses and desktop/device adapters.

Combined checks also reverse header order and enable implementations after an
initial declaration-only pass. The complete inventory includes four explicitly
reserved, comment-only headers; these have no API or implementation and cannot
be dependencies of working modules.

Extension suites cover DNS/cache resolution, HTTP application composition,
FTP/SMTP/IMAP sessions, native event descriptors, task/process scheduling,
virtual filesystems, compiler transformations, native code generation, and
embedded language/audio/browser utilities. Native tests use local resources,
not public network services; documented platform or permission restrictions
remain distinct from tested functionality.

Supplementary checks added here:

- `tests/test_hardening.c` uses a tracked allocator whose reallocations **always
  move**, checks self-appending vector/stack/wrapped queue/deque storage, and
  injects failures at each allocation position in twelve parsing/rendering paths.
  It also checks constructor failure state, canonical decoding, numeric extremes,
  S-expression growth and ownership mistakes, roff text and interpreter limits.
- `tests/test_hostile_inputs.c` runs deterministic truncated syntax seeds and
  2,000 pseudorandom byte inputs through a shared bounded fuzz harness.
- `tests/fuzz_headers.c` is an optional Clang libFuzzer target. It exercises
  text/configuration/grammar/markup/protocol parsers, terminal decoders, address
  parsing, binary frame/signature readers, encoding and formatting utilities.
  Individual inputs are capped at 1,024 bytes. This is a smoke-test resource bound,
  not a restriction imposed on the public library APIs.
  Extension coverage includes DNS names/messages, MicroJS, WAV, POD, cron, lexical
  VFS/file-server paths, architecture JSON and FTP/SMTP/IMAP command sessions.
  Valid DNS/WAV seeds and every truncated prefix accompany the text seeds.
- `clang --analyze -std=c11 -Iinclude tests/all_headers.c` analyzes the combined
  implementation translation unit. A clean result only covers enabled checkers.

## Strengthened contracts

Additional refinement regressions cover DNS query/reply buffer overlap, pending
partial SMTP replies, exact-capacity FTP commands, taskpool callback reentry,
process-child ownership, bounded filesystem scans, malformed instruction-selection
tables, contradictory architecture metadata, overlapping PCM subviews, and
MicroJS decimals with compensating exponents and subnormal results. These focused
checks do not expand the documented protocol or instruction-set subsets.

Public function signatures and implementation-switch ownership remain unchanged.
The most important behavior changes are intentional stricter validation:

1. Vector/stack push accept borrowed elements across growth. Queue/deque push
   accept an existing aligned live slot, including after wraparound. Failure
   preserves length and existing elements. Internal source ranges outside live
   storage are rejected. Arbitrary overlapping external buffers are not promised.
2. Bitset, bitmap, bloom, grid, graph, matrix, ring, disjoint-set, moving-average
   and histogram constructors allocate before publishing destination fields.
   Failure leaves the destination unchanged. Initialize new objects normally;
   **successful reinitialization of a live owner is not a replacement operation**:
   free the old object first. Empty grid/matrix dimensions allocate a small valid
   backing block consistently rather than relying on `calloc(0, ...)` behavior.
3. Base64, hex, percent and RLE decoding validate the complete input and capacity
   before writing. Failure leaves output and optional written count unchanged.
   Base64/hex/percent support exact in-place shrinking; RLE requires disjoint
   buffers. Empty NULL input/output is accepted by these decoders at length zero.
4. Semver's numeric-triplet subset rejects signs, whitespace, leading zeroes,
   overflow and suffixes. CSV rejects junk after quoted fields and quotes inside
   unquoted fields, and checks growth arithmetic. Earlier successful callback
   rows remain delivered if a later row fails; there is no transaction rollback.
5. S-expression parsing distinguishes actual closing quotes from escaped/opening
   quotes. Rendering reserves before every delimiter and checks arithmetic;
   whitespace atoms roundtrip. Destruction is iterative. Push rejects self/ancestor
   cycles and duplicate insertion into the same list; cross-owner sharing remains
   prohibited by the caller-owned tree contract. Render depth remains 128.
6. MicroTcl releases all words on its 64-word limit and every error path, handles
   all inter-word whitespace, checks expansion sizing, preserves escaped dollars,
   rejects empty integer operands and saturates its error counter at `INT_MAX`.
   Quoted/braced words must end at a separator. Earlier commands are not rolled
   back on later failure; this remains a small interpreter, not a sandbox.
7. Histograms require finite endpoints and avoid NaN-to-integer conversion for
   huge finite spans. Interpolation preserves endpoints and handles opposite-sign
   extremes without avoidable subtraction overflow. HSV hue wraps negative/multiple
   rotations correctly. Primality now uses bounded deterministic 64-bit
   Miller-Rabin instead of potentially billions of trial divisions; it is neither
   cryptographic nor constant-time.
8. Manpage sizing counts the repeated title and checks cumulative arithmetic;
   text beginning with roff control characters is protected. Archive writers
   preserve a source borrowed from their own byte buffer across header growth.
9. Lazy pipelines reject construction beyond 256 stages rather than allowing
   an unbounded recursive `next` call chain. Failed adapter construction leaves
   upstream ownership with the caller, as before.

## Reproduce

```sh
python3 tools/check.py
CC=clang CXX=clang++ python3 tools/check.py --sanitizers
cmake -S . -B build -DMULIB_BUILD_MODULE_EXAMPLES=ON
cmake --build build
ctest --test-dir build --output-on-failure
clang --analyze -std=c11 -Iinclude tests/all_headers.c
```

Optional mutation fuzzing, with artifacts outside the source tree:

```sh
cmake -S . -B /tmp/opencode/mulib-fuzz-build -DCMAKE_C_COMPILER=clang \
  -DMULIB_BUILD_TESTS=OFF -DMULIB_BUILD_FUZZER=ON
cmake --build /tmp/opencode/mulib-fuzz-build
/tmp/opencode/mulib-fuzz-build/mulib_fuzz_headers -runs=100000 \
  -len_control=0 -max_len=1024 -timeout=10 -artifact_prefix=/tmp/opencode/mulib-fuzz-
```

No native input injection, guest execution, subprocess launch or desktop server
connection is performed by the hostile-input harness. Platform success paths
still require separate environment/hardware testing. Mutable public structures
must be initialized, valid and externally synchronized; no C API can validate
arbitrary dangling pointers or caller-forged allocation capacities.

`examples/calculator.c` is a runnable MicroJS integration example, registered
alongside `quickstart` and `dogfood` when module examples are enabled.
