# 13. Errors, resource limits, and security

## Error categories

Recoverable source/format/execution failures return diagnostics. Invalid IL,
unknown instructions, malformed ELF records, overflowed relocation fields,
nonterminating rewrites under a budget, and runtime arithmetic traps are not
reported as successful artifacts.

C++ API misuse can still throw. Examples include constructing an empty rewrite
variable, asking a value for the wrong kind, accessing an absent result, or
constructing native DSL register 32. Allocation failures and application
callback exceptions follow the normal C++ exception policy, except that VM
instruction/backend exceptions are caught at their documented dispatch
boundaries. There is no advertised `-fno-exceptions` mode.

Do not wrap every failure in an empty catch block and continue with returned
bytes. Check the result boolean and handle diagnostics. In particular, a
retained partial execution/rewrite value does not mean the operation succeeded.

## Default resource limits

| Area | Default |
|---|---|
| IL source | 64 MiB |
| IL instruction count | 1,000,000 |
| VM attempted instructions | 10,000,000 |
| MetaIL stack values | 1,000,000 |
| Completed traces | 1024 |
| One trace length | 256 |
| JIT method compilation attempts | 128 |
| JIT executable mappings, including page padding | 16 MiB |
| One JIT-generated assembly source | 64 MiB |
| Parser input | 64 MiB |
| Compiled grammar bodies/patterns | 1 MiB |
| Parser expression operations | 1,000,000 |
| Parser pending events | 1,000,000 |
| Parser structural depth | 256 |
| Text term/rule source | 64 MiB |
| Text term nodes / depth | 1,000,000 / 256 |
| Rewrite replacements | 10,000 |
| Rewrite result nodes / depth | 1,000,000 / 256 |
| Assembly source / final output | 64 MiB / 64 MiB |
| Link input per object | 256 MiB |
| Link aggregate input | 512 MiB |
| Link object count | 1024 |
| Link output image | 256 MiB |
| ELF section/symbol spelling | 4096 bytes |

A mutated or adversarial input can legitimately turn a straight-line program into
a self-loop. The step budget is what stops it; the default ten million
instructions is a generous correctness bound, not a fast failure mode. Choose a
lower ceiling for interactive or multi-tenant use.

Limits are configurable through their respective options structures. They are
logical bounds, not precise memory-accounting quotas. Multiple owning copies,
diagnostics, hash tables, symbol strings, ASTs, and rewrite history add overhead.
Facade operations may create temporary component copies. MCIL codecs and
lower-level helpers retain their own default verification ceilings unless an
explicit limits argument is available; raising one configuration field does
not automatically expand every other stage's limits.

Choose coordinated limits for an application workload. For example, a generated
native assembly file can be much larger than its source IL, and ELF headers,
symbol tables, relocation tables, and page padding increase artifact size.
It is legitimate for one stage to reject the output of an earlier stage under
tighter configured bounds.

## Arithmetic and binary robustness

Data field widths are checked rather than truncated. Signed IL overflow is
implemented through unsigned bit arithmetic. Division and shifts check invalid
cases. Binary decoders check lengths before field reads. ELF alignment and
address calculations guard overflow; relocation arithmetic avoids signed UB
and compiler-specific wide integer requirements.

ELF relocation writes are bounded by the contributing input section, preventing
a malicious offset from reaching another input contribution after merging.
Weak and strong symbols are resolved explicitly. Unknown relocations, dynamic
features, and unsupported allocated flags cause errors instead of being skipped
under an assumption that their effects are unimportant.

Generated executable permissions separate read-only headers from executable
code and writable data, and the stack is marked non-executable. Inputs requesting
writable executable sections are rejected. These are useful defenses, not a
proof that arbitrary machine code is safe.

## Limits that are not sandboxes

* `std::regex` may do expensive internal backtracking beyond parser operation
  counts. Use trusted linear-time patterns, or isolate regex parsing in a
  separately resource-limited process.
* Generic C++ terms/ASTs can be constructed with extreme nesting before the
  library sees them. Recursive value destruction/copying and direct helper
  calls can exhaust the C++ stack. Do not feed adversarial C++ object graphs to
  unbounded helpers; parse bounded textual representations instead.
* A rewrite can allocate a large replacement before the result's structural
  limit is checked. Rule expansion and retained cycle history require memory
  planning. Rules are application configuration, not hostile executable code
  that the library can prove inexpensive.
* Ahead-of-time native stack values consume the process's real stack. Its OS
  limit may be lower than the logical limit. Native I/O can block or be terminated
  by signals. The in-process JIT uses a checked vector stack and captured output.
* A custom ISA or JIT backend can mutate raw pointers, bypass stack checks, or
  execute arbitrary machine code. Its correctness belongs to the embedding host.
* File output helpers are not atomic and may leave partial files on I/O failure.
  Do not publish artifacts until your own staging/rename policy completes.

For multi-tenant or otherwise untrusted execution, use process isolation,
operating-system resource controls, timeouts, file-descriptor discipline, and
appropriate privilege separation. A verified IL shape is not authorization to
run user programs with unrestricted host access.

## Callbacks and lifetimes

Listener reference subscriptions borrow the listener. The caller must retain
it for as long as the stream may emit. Callback values are owned by the stream,
but lambda captures may themselves borrow other objects. Snapshotting the
subscription vector protects iteration from subscriptions added during an
event; it does not extend borrowed-object lifetimes or make callbacks thread-
safe.

Do not recursively mutate a parser/system/VM from its own callback. Parser
reentrant `parse` is rejected. Other component state mutation during execution
is outside the supported contract. Use queues to deliver observations to a
different thread or stage rather than changing the active traversal.

## Artifact trust and deployment

Artifact-building APIs return bytes and never execute them automatically.
`Toolchain::jit` and `metajit::Engine::run` explicitly opt into in-process native
execution. Keep that boundary in build services. Record source/configuration provenance, preserve
diagnostics, validate target architecture/ABI, and use an external deployment
approval step where required. Verify permissions and paths independently of
source-provided labels or filenames.

There is no claim of third-party security certification, exhaustive formal
verification, or compatibility with all ELF producers. Production readiness for
a deployment includes testing that deployment's exact compiler, object flags,
resource limits, runtime environment, and failure-handling policy.
