# MuLIB module and composition guide

MuLIB has 193 implemented single-header C11 modules. Every implemented module has a corresponding
`manual/mulib-<name>.md` and `examples/modules/<name>.c`. The machine-readable
inventory is `MANIFEST.yaml`. Names are not promises of full external standards;
consult each manual's exact supported syntax, errors, and limits.

## New module groups

| Area | Modules |
| --- | --- |
| Memory and portability | [alloc](mulib-alloc.md), [mempool](mulib-mempool.md), [strpool](mulib-strpool.md), [memtkx](mulib-memtkx.md), [port](mulib-port.md) |
| Parsing | [parsec](mulib-parsec.md), [peg](mulib-peg.md), [regex](mulib-regex.md), [atn](mulib-atn.md), [glr](mulib-glr.md), [sppf](mulib-sppf.md), [gss](mulib-gss.md) |
| Trees and streams | [ast](mulib-ast.md), [visitor](mulib-visitor.md), [listener](mulib-listener.md), [evtstream](mulib-evtstream.md), [lazystream](mulib-lazystream.md) |
| Configuration | [confuse](mulib-confuse.md), [ini](mulib-ini.md), [toml](mulib-toml.md) |
| Structured data | [json](mulib-json.md), [yaml](mulib-yaml.md), [toml](mulib-toml.md), [template](mulib-template.md), [xml](mulib-xml.md), [xpath](mulib-xpath.md) |
| Documents and graphics | [html](mulib-html.md), [css](mulib-css.md), [markdown](mulib-markdown.md), [svg](mulib-svg.md), [graphviz](mulib-graphviz.md), [vector algebra](mulib-vector.md) |
| Storage and native formats | [archive](mulib-archive.md), [mmfile](mulib-mmfile.md), [kvdb](mulib-kvdb.md), [elf](mulib-elf.md), [dynload](mulib-dynload.md), [drm](mulib-drm.md) |
| Terminals | [terminfo](mulib-terminfo.md), [termkey](mulib-termkey.md), [termutils](mulib-termutils.md), [vterm](mulib-vterm.md), [tui](mulib-tui.md) |
| Protocols | [uri](mulib-uri.md), [http](mulib-http.md), [rpc](mulib-rpc.md), [dbus](mulib-dbus.md) |
| IP addresses | [IPv4](mulib-inet4.md), [IPv6](mulib-inet6.md), [historical inte6 header](mulib-inte6.md), [address lookup](mulib-getaddr.md) |
| Input and devices | [input](mulib-input.md), [keyremap](mulib-keyremap.md), [marionette](mulib-marionette.md), [udev](mulib-udev.md), [vdev](mulib-vdev.md), [kvm](mulib-kvm.md) |
| Desktop integration | [wayland](mulib-wayland.md), [wlroots geometry](mulib-wlroots.md), [xlib](mulib-xlib.md) |
| Runtime utilities | [minivm](mulib-minivm.md), [minish](mulib-minish.md), [Python bindings generator](mulib-pybind.md), [Ruby bindings generator](mulib-rubybind.md) |
| Indexes and differences | [avl](mulib-avl.md), [b+tree](mulib-b+tree.md), [hattrie](mulib-hattrie.md), [txtdiff](mulib-txtdiff.md), [mmdiff](mulib-mmdiff.md) |
| DNS | [wire format](mulib-dns.md), [client](mulib-dnsclient.md), [cache](mulib-dnscache.md), [resolver](mulib-dnsresolver.md), [nameserver](mulib-nameserver.md), [historical dnsnmeserver facade](mulib-dnsnmeserver.md) |
| HTTP applications | [webclient](mulib-webclient.md), [webframework](mulib-webframework.md), [fileserver](mulib-fileserver.md), [reverse proxy](mulib-revproxy.md), [minicurl](mulib-minicurl.md), [miniwget](mulib-miniwget.md), [minincat](mulib-minincat.md) |
| Mail and file transfer | [FTP client](mulib-ftpclient.md), [FTP server](mulib-ftpserver.md), [SMTP client](mulib-smtpclient.md), [SMTP server](mulib-smtpserver.md), [IMAP server](mulib-imapserver.md), [IMAP client (impaclient)](mulib-impaclient.md) |
| Events and native OS | [asyncio](mulib-asyncio.md), [epoll](mulib-epoll.md), [select](mulib-select.md), [eventfd](mulib-eventfd.md), [timerfd](mulib-timerfd.md), [perf](mulib-perf.md), [signal](mulib-signal.md), [syscall](mulib-syscall.md), [fswatch](mulib-fswatch.md) |
| Tasks and services | [taskpool](mulib-taskpool.md), [taskdag](mulib-taskdag.md), [parallel](mulib-parallel.md), [pipeline](mulib-pipeline.md), [process](mulib-process.md), [cron](mulib-cron.md), [initsys](mulib-initsys.md) |
| Filesystem abstractions | [VFS](mulib-vfs.md), [FUSE adapter](mulib-fuse.md), [procfs](mulib-procfs.md), [sysfs](mulib-sysfs.md), [pathdb](mulib-pathdb.md) |
| Compiler transformations | [MIR](mulib-mir.md), [C machine](mulib-cmachine.md), [VM weaving](mulib-vmweave.md), [C weaving](mulib-ccweave.md), [tree selection](mulib-muburg.md), [instruction scheduling](mulib-isched.md), [egraph](mulib-egraph.md), [rewrite](mulib-rewrite.md) |
| Machine code | [dsymspec](mulib-dsymspec.md), [assembler](mulib-assembler.md), [emitter](mulib-emitter.md), [linker](mulib-linker.md), [JIT](mulib-jit.md), [dynamic calls](mulib-dyncall.md) |
| Embedded languages and tools | [MicroJS](mulib-microjs.md), [Datalog](mulib-datalog.md), [Perl POD](mulib-perlpod.md), [microaudio](mulib-microaudio.md), [browser tools](mulib-browsertools.md) |

`mop`, `mune`, `wask`, and `wmfunc` are reserved headers, not implemented APIs. The
manifest distinguishes the complete header inventory from the implemented count;
the checker permits only comment-only reserved files and rejects dependencies on
them. Native modules document platform restrictions. Protocol modules are
focused building blocks, not a claim of complete standards conformance, TLS,
authentication, or safe public deployment.

## Dependency ownership

MuLIB does not automatically turn on dependency implementation switches.
This avoids duplicate symbols when independent modules share a dependency.
An application may put all implementation switches in one C11 source file,
before including any header, and include declarations everywhere else.

For JSON and consumers of its value model, the foundation is:

```c
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_JSON
#include <mulib-json.h>
```

Add the consumer's switch and header in that same translation unit. Its manual
may list additional dependencies. A JSON value is owned by its creator until a
successful append/set transfers ownership. Borrowed children must not be freed
separately. Strings and object keys are length-delimited and may contain NUL.
Serialize to an owned `char *` and release it with `free`.

Memory allocators are per-instance value objects, never process-global hooks.
For example, passing a `mulib_memtkx_allocator` view to a memory pool or string
pool measures its real allocations. The tracker must outlive those objects.

## Validation

See [header hardening](hardening.md) for failure-atomic constructors/decoders,
alias-safe growth, allocation-failure sweeps and optional mutation fuzzing.

`python3 tools/check.py` validates inventory, runs examples and focused tests,
compiles headers in isolation with their transitive dependencies, and checks
all implementations together. Declaration-only C++17 inclusion is checked when
a C++ compiler is available. Tests of wire encoders and OS failure paths are
not a claim of interoperability with every service or hardware device.

The C implementation is the supported implementation target. Linux device modules
use native APIs. Wayland/Xlib client helpers optionally load their native runtimes;
their pure utilities do not require those libraries. Build and runtime platform
restrictions are documented per module, and hardware is not required for the
normal test suite.

The separate-translation-unit `tests/composition.c` also exercises one value
model across JSON parsing, YAML emission/parsing, TOML emission/parsing, and
template rendering, alongside tracked memory pools, interned strings, and
numerical vectors. This checks real cross-module reuse rather than merely
including unrelated headers in one file.

`tests/extensions.c` exercises an in-memory VFS through a fileserver, router and
HTTP wire exchange, downloads binary data with `miniwget`, and evaluates a
downloaded expression with MicroJS. Its implementations live in a separate
translation unit. Transport is an explicit local callback; this test demonstrates
composition without claiming socket, TLS or remote-service interoperability.
