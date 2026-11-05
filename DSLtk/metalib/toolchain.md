# AST and toolchain libraries

These opt-in libraries have matching C++ and D host adapters and shared MetaML
modules. Install them before importing their `mod.mml` files:

```cpp
#include "metalib/cpp/Toolchain.h"

metaml::Session session;
metalib::install_toolchain(session);
session.allow_all_primitives(); // trusted programs only; otherwise use an allowlist
session.load_stdlib("metalib");
session.eval("import \"ast\"\nimport \"system\"\nimport \"objfile\"\nimport \"xvm\"");
```

In D, import `metalib.d.Toolchain` and call `installToolchain(session)` (also
exported as `install_toolchain`). The portable `install_all` bundle and the
default CLIs do **not** install these filesystem/code-execution services.
Individual installers are idempotent, install their dependencies, and never
grant primitive capabilities:

| Import | C++ header / installer | D module / installer |
| --- | --- | --- |
| `visitor` | `metalib/visitor/Visitor.hpp`, `install_Visitor` | `metalib.visitor.Visitor`, `installVisitor` |
| `ast` | `metalib/ast/Absyn.hpp`, `install_Absyn` | `metalib.ast.Absyn`, `installAbsyn` |
| `system` | `metalib/system/System.hpp`, `install_System` | `metalib.system.System`, `installSystem` |
| `objfile` | `metalib/objfile/Objfile.hpp`, `install_Objfile` | `metalib.objfile.Objfile`, `installObjfile` |
| `arch` | `metalib/arch/Arch.hpp`, `install_Arch` | `metalib.arch.Arch`, `installArch` |
| `chunk` | `metalib/chunk/Chunk.hpp`, `install_Chunk` | `metalib.chunk.Chunk`, `installChunk` |
| `xvm` | `metalib/xvm/XVM.hpp`, `install_XVM` | `metalib.xvm.XVM`, `installXVM` |

## Absyn and Visitor

`import "ast"` defines `Absyn`, with the visitor module attached as
`Absyn.Visitor`. `import "visitor"` independently defines `Visitor`.

* `Absyn.of_source : String -> Result Ast String`
* `Absyn.of_file : String -> Result Ast String`
* `Absyn.current : Unit -> Result Ast String`
* `Absyn.tag`, `text`, `children`, `dump`: structural access and canonical dump.

`Ast` is the existing `Leaf String String | Node String [Ast]` tree type. Parsing
returns actual trees, not strings containing trees. The root is `Node "Program"`;
its children use the same tags as MetaML's canonical AST dump. This is the
syntactic AST, not a typed compiler IR; source positions are not encoded in `Ast`.
`ASTBuilder` primitives can construct and modify these same values.

`current ()` means the **innermost executing program**, including the expression
calling it. Nested imports and evaluations temporarily replace that context and
restore it on exit, including on errors. It does not reread the file from disk
and does not mean the session's entire history.

```ml
import "ast"
val count_nodes source := match Absyn.of_source source with
  | Ok tree -> Ok (Absyn.Visitor.fold (fn count _ -> count + 1) 0 tree)
  | Err error -> Err error
```

Visitor operations:

* `walk {enter, leave} tree`: `enter : Ast -> Bool` runs before children. False
  prunes the children. `leave : Ast -> Unit` still runs for that node, afterwards.
  Callback exceptions propagate; `leave` is not an exception-finalizer.
* `preorder callback tree`, `postorder callback tree`: visit all nodes in order.
* `fold step initial tree`: preorder, left-to-right state accumulation.
* `transform rewrite tree`: bottom-up rewriting; a replacement is not revisited.

Traversal is recursive and uses the runtime's normal depth limits.

## System, OS and implementations

`System` is a functor over `OS`:

```ml
import "system"
module Unix := System(Posix)
Unix.cwd ()
```

`Posix`, `Win32` and `MacOS` implement `OS`. They are separately importable as
`"posix"`, `"win32"` and `"macos"`, with matching host entry headers/modules.
`Posix` is available on Unix including macOS; `MacOS` only on macOS; `Win32` only
on Windows. Target descriptors are usable anywhere, but unavailable OS operations
return `Err`, rather than silently executing operations on a different OS.

`OS` exposes `name`, `available`, `separator`, `cwd`, `getenv`, `read_file` and
`write_file`. The last four return `Result`. `getenv` returns `Ok None` for an
absent variable, distinguishing absence from an empty value. File contents are
binary byte strings, preserving NUL and line endings. Writes truncate/create;
they are not atomic. Paths and environment variable names reject embedded NUL;
names also reject `=` and empty strings. Host failure messages can vary by OS.
These are host filesystem services, not a filesystem sandbox or OS emulator.

## Objfile, OBJFILE and formats

```ml
import "objfile"
import "arch"
module Object := Objfile(ELF)
val image := Object.of_code "amd64" (Amd64.return_int 42)
val info := match image with | Ok bytes -> Object.inspect bytes | Err error -> Err error
```

`ELF`, `PE`, `COFF` and `MachO` implement `OBJFILE`, and are separately
importable as `"elf"`, `"pe"`, `"coff"` and `"macho"`. `Objfile(Format)` forwards
that signature:

* `name : Unit -> String`
* `matches : String -> Bool`: successful container inspection, not just a magic
  prefix test.
* `inspect : String -> Result ObjInfo String`: `ObjInfo` has `format`, `arch`,
  `bits`, `sections` fields. Header, section-table and section-payload bounds are
  checked before access. This is not a complete linker/loader validation pass.
* `of_code : String -> String -> Result String String`: architecture name and
  raw machine code to a minimal single-text-section image.

Inspection supports ordinary ELF32/64 with either byte order, COFF, PE32/PE32+,
and thin Mach-O32/64. Extended ELF section counts, fat Mach-O and COFF bigobj
are not supported. Unknown ELF/Mach-O machine IDs report `arch = "unknown"`;
COFF inspection currently requires one of the three supported 64-bit machines.

Emission produces ELF64, COFF or Mach-O64 **relocatable objects**, or a minimal
PE32+ **image** with an entry RVA of `0x1000`. Objects have no symbols,
relocations, imports or exports. This is not a linker. PE code must conform to
the platform entry-point ABI; `return_int` is a callable function, not a complete
portable executable entry point. AMD64 and Aarch64 are supported in all formats;
RISC-V64 is supported in ELF/COFF/PE, but not Mach-O.

## Arch and ARCH

`Arch(Target)` forwards the `ARCH` signature implemented by `Amd64`, `Riscv64`
and `Aarch64`; separate imports are `"amd64"`, `"riscv64"`, `"aarch64"`.
The signature exposes `name`, `word_bits`, `endian`, `native`, and
`return_int : Int -> String`. All three targets are 64-bit little-endian.
`return_int` emits a native ABI function returning the supplied 64-bit integer.
Emission does not execute code and works for non-host targets too. `native ()`
reports a matching host CPU, **not** permission to allocate executable memory.

Host-level `arch::jump` / D `jump` emit ABI-transparent tail-jump stubs used by
XVM. These entry stubs preserve the incoming arguments and return address.

## Chunk and XVM

```ml
import "xvm"
module VM := XVM(Amd64)
val result := match Chunk.of_source "val make x := fn y -> x + y\nmake 40 2" with
  | Ok chunk -> (match VM.run chunk with | Ok value -> Ok (VM.show value) | Err error -> Err error)
  | Err error -> Err error
```

`Chunk` wraps the opaque `XvmChunk` type (`Chunk.t`):

* `of_source : String -> Result Chunk.t String`: validate syntax and construct a
  reusable chunk. Type checking occurs in the destination session at execution.
* `of_file : String -> Result Chunk.t String`: read an encoded XVM chunk if it
  begins with `XVM\0`, otherwise compile a MetaML source file.
* `current : Unit -> Result Chunk.t String`: snapshot the current program's
  source and filename, without reading disk. Running this chunk executes the
  **whole** source again; it is not a continuation and can recurse if that source
  runs its own current chunk.
* `source`, `filename`: inspect the captured source identity.
* `encode : Chunk.t -> String`, `decode : String -> Result Chunk.t String`:
  architecture-neutral, versioned byte strings. Persist with an OS's `write_file`.
  Decoding rejects unknown versions/opcodes, truncation and trailing data.

Chunks never store serialized host pointers, closures or mutable checker cells.
Reusing a chunk reparses/rechecks into the destination session so bindings cannot
leak between sessions. The original filename is retained for relative imports
and diagnostics. Decoded source still requires the normal runtime capabilities.

`XVM(Target)` exposes `run`, `jit : Chunk.t -> Result XvmValue String` and
`show : XvmValue -> String`. Execution uses the **calling session**, with its
bindings, output, imports and capability allowlist. The language boundary wraps
results in an opaque `XvmValue`, rather than unsafely claiming a string can
produce any polymorphic type. Closures/modules/foreign values are retained as
runtime values, not converted to strings until `show` is called.

### Backend and native tier

XVM is now the default core executor. All checked expression kinds lower to
stack bytecode and native **helper-call blocks**, preserving closures, recursive
bindings, ADTs, pattern matching, records, lazy modules/functors, imports,
directives and primitive behavior without calling the tree evaluator. The v1
chunk envelope remains source-backed so checking and lowering occur in the
destination session. `run` selects portable compiled execution; `jit` requires
native blocks. This is a subroutine-threaded JIT, not a register-allocating or
optimizing compiler; complex value operations use runtime helpers. No speedup
over the evaluator is promised. [XVM.md](../XVM.md) specifies the engine, limits,
cache invalidation, native ABI and command-line selection.

The JIT only executes code for the host architecture. Foreign targets return
`Err`. Allocation uses write-only-at-construction then read/execute protection,
never RWX pages. Instruction caches are synchronized. Security policies may deny
executable mappings (notably hardened macOS processes); this returns an error,
and `run` remains available without a JIT. Generated code is internal/trusted;
there is no API for executing arbitrary decoded machine-code buffers.

Host backends preserve typed runtime values and complete diagnostic reports:

```cpp
#include "metaml-backends/XVM.hpp"

metaml::Session session;
metaml::backends::XVM backend;
auto report = backend.eval(session, "val f x := x + 1\nf 41", "example.mml", true);
// report.value contains the actual MetaML value; false selects non-JIT dispatch.
```

In D, import `metalib.xvm.Backend` and construct `new Backend`. Install the
standard library/host services into the destination session if its programs
need them. The backend does not grant capabilities or automatically load a
prelude. D backend/VM `close()` closes the adapter; executable caches now belong
to the destination interpreter and are released with `Session.close()`/GC.
C++ uses RAII. Sessions and VM instances require caller synchronization.

## Verification

`metalib_toolchain` and `metalib_toolchain_d` CTest targets cover the shared
language contracts, traversal order/pruning/rewriting, independent imports,
functor composition, object round trips and malformed data, chunk persistence,
current-program restoration, nested error recovery, capability enforcement,
interpreter/JIT equivalence and full-language runtime values.

Native execution is tested on the current host only; other target encodings
require target hardware or emulators for execution testing.
