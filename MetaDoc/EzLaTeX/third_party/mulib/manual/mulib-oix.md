# mulib-oix

OIX is a freestanding, cooperative, single-address-space operating system for a
16-bit x86 guest. `mulib-oix.h` contains its native kernel, configuration compiler,
image validator, and a Linux x86 KVM virtual machine monitor. Building the image
requires C11, libc, and MuLIB headers; no external libraries, assembler, linker,
BIOS, guest Linux, or subprocess is used. The default serialized image is 639
bytes, including a 532-byte native kernel, task, banner, and 64-byte header.

OIX supplies a round-robin scheduler, per-task 16-bit accumulator, console input
and output, a read-only filesystem of embedded named byte strings, and task exit
statuses. Task programs use the small instruction set below. The native guest
kernel interprets those programs inside the VM; the host handles port I/O only.
OIX does not implement POSIX, Linux executables, native user programs, processes,
preemption, memory protection between tasks, writable files, networking, or a
bootloader. An `.img` file uses the OIX loader format; it is not a BIOS disk or
Linux `bzImage`. Use `mulib_oix_vm_create` to load it.

## Build and run

The [standalone example](../examples/modules/oix.c) is also a small compiler and
runner:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude examples/modules/oix.c -lm -o oix
./oix --output oix.img
./oix --json config.json --output custom.img
./oix --yaml config.yaml --run
./oix --toml config.toml --run
./oix --confuse config.conf --run
```

With no arguments it compiles and validates the default image without opening
KVM or writing files. `--output` writes the serialized bytes. `--run` requires a
Linux x86 KVM host with access to `/dev/kvm`, seeds console input with `hello\n`,
runs with a 100,000-exit budget, and returns the guest exit code. Configuration
input is limited to 1 MiB. It reports unavailable KVM and failed builds explicitly.

Define these switches in exactly one translation unit before including the
header (the example does so):

```c
#define MULIB_IMPL_OIX
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_KVM
#define MULIB_IMPL_JSON
#define MULIB_IMPL_YAML
#define MULIB_IMPL_TOML
#define MULIB_IMPL_CONFUSE
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_GLOB
#include "mulib-oix.h"
```

Direct dependencies are emitter, crc32, kvm, json, yaml, toml, confuse, and base64;
the remaining switches supply their parser/storage dependencies. OIX never enables
another implementation switch implicitly. Image compilation and validation are
portable; execution is available only on Linux x86. Link `-lm` for the existing
configuration parsers when required by the toolchain.

## C configuration

Initialize a configuration, supply borrowed file/task arrays, then compile:

```c
mulib_oix_config_t config;
mulib_oix_image_t image = MULIB_OIX_IMAGE_INIT;
mulib_oix_error_t error;
const mulib_oix_file_t files[] = {{"/motd", "Hello from OIX\n", 15}};
const mulib_oix_instruction_t code[] = {
  {MULIB_OIX_CAT, 0, NULL, 0, "/motd"},
  {MULIB_OIX_SET, 42, NULL, 0, NULL},
  {MULIB_OIX_NUMBER, 0, NULL, 0, NULL},
  {MULIB_OIX_EXIT, 0, NULL, 0, NULL}
};
const mulib_oix_task_t tasks[] = {{"init", code, 4}};
mulib_oix_config_init(&config);
config.name = "MyOIX";
config.banner = "";
config.files = files;
config.file_count = 1;
config.tasks = tasks;
config.task_count = 1;
if (mulib_oix_compile(&config, &image, &error) == MULIB_OIX_OK) {
  /* Save image.data[0..image.length), validate it, or create a VM. */
  mulib_oix_image_free(&image);
}
```

The example stores the 15 message bytes without a terminating NUL. Use
`sizeof(message)-1` for a string literal stored in a named array. Binary data and NUL
bytes are supported in file contents and WRITE data. Names and banners are C
strings. All unused instruction members must be zero or NULL.

Defaults are name `OIX`, banner `OIX ready\n`, no files, and one `init` task that
exits with code zero. `config.size` must equal `sizeof(config)` and reserved fields
must remain zero. Compilation copies all data; the original configuration and
buffers may be changed or freed after the call. Output must be initialized empty;
on failure it is unchanged. Free/reset an image with `mulib_oix_image_free`, which
accepts NULL and repeated calls. Do not copy an owning image by value.

## Configuration formats

All text formats map to the same schema. Unknown fields, invalid operand types,
unknown instructions, missing files, duplicate names, and invalid jump targets
are rejected. Omitted root fields retain defaults. An explicit empty `tasks`
array is invalid. JSON/YAML/TOML retain their respective MuLIB parser profiles;
see their manuals for grammar limits and unsupported features. Confuse uses
`titled file/task` sections and repeated untitled `instruction` sections.

JSON:

```json
{
  "name": "MyOIX",
  "banner": "",
  "files": [{"name": "/motd", "data": "Hello from OIX\n"}],
  "tasks": [{"name": "init", "code": [
    {"op": "cat", "file": "/motd"},
    {"op": "exit", "value": 0}
  ]}]
}
```

YAML:

```yaml
name: MyOIX
banner: ""
files:
  - name: /motd
    data: "Hello from OIX\n"
tasks:
  - name: init
    code:
      - op: cat
        file: /motd
      - op: exit
        value: 0
```

TOML:

```toml
name = "MyOIX"
banner = ""
[[files]]
name = "/motd"
data = "Hello from OIX\n"
[[tasks]]
name = "init"
code = [{op="cat", file="/motd"}, {op="exit", value=0}]
```

Confuse:

```confuse
name = "MyOIX"
banner = ""
file "/motd" { data = "Hello from OIX\n" }
task "init" {
  instruction { op = cat file = "/motd" }
  instruction { op = exit value = 0 }
}
```

Call `mulib_oix_compile_text(format, source, length, &image, &error)` with one of
`MULIB_OIX_JSON`, `MULIB_OIX_YAML`, `MULIB_OIX_TOML`, `MULIB_OIX_CONFUSE`. The source
is borrowed for the call and may be freed afterwards. File records require
`name` and exactly one of `data` (a string) or `data_base64`. WRITE instructions
require exactly one of `text` or `data_base64`. Standard padded base64 allows
binary payloads in every format; for example `"data_base64":"AP8="` encodes
bytes 0 and 255. Confuse scalar values are strings or unsigned 16-bit integers;
duplicate keys and conflicting section fields are rejected.

## Instructions and scheduling

Every task has its own unsigned 16-bit accumulator, initially zero. The guest
executes one instruction from each live task in configuration order, then repeats.
An instruction's entire output is emitted before the next task instruction.

| Text `op` / C opcode suffix | Operand | Effect |
| --- | --- | --- |
| `write` / WRITE | `text` or `data_base64`; C `data,length` | Write exact bytes. |
| `cat` / CAT | `file` | Write the entire embedded file. |
| `set` / SET | `value`, default 0 | Set accumulator. |
| `add` / ADD | `value`, default 0 | Add modulo 65536. Use 65535 to subtract one. |
| `jump` / JUMP | `target` | Jump to zero-based instruction index in this task. |
| `jnz` / JNZ | `target` | Jump if accumulator is nonzero. |
| `read` / READ | none | Consume one console byte into accumulator; retry on later turns if empty. |
| `putc` / PUTC | none | Write low byte of accumulator. |
| `number` / NUMBER | none | Write unsigned decimal accumulator, including zero. |
| `yield` / YIELD | none | Take a scheduler turn without work. |
| `exit` / EXIT | `value`, default 0, maximum 255 | Exit this task. |
| `size` / SIZE | `file` | Set accumulator to file byte length. |
| `byte` / BYTE | `file` | Read file byte at accumulator offset into accumulator. |
| `list` / LIST | none | Write each filename followed by newline, in configuration order. |
| `info` / INFO | none | Write OS name followed by newline. |

Scalar operands and jump targets are integers 0..65535, never fractional. In C,
`value` carries the scalar or target index. `file` is an absolute guest name, not
a host path. Jump targets must remain in the same task. Falling off a task's
program, or reading outside a file with BYTE, exits that task with status 255.
The first nonzero task status in execution order becomes the guest status.
Once every task has exited, the kernel signals completion to the host.

A blocked READ still emits scheduler ticks, allowing other tasks to progress
and the host exit budget to remain effective. Yielding is cooperative and there
are no guest interrupts, timer preemption, priority classes, or idle sleep.

## Limits and image validation

| Resource | Limit |
| --- | --- |
| Guest physical RAM | 65,536 bytes |
| Kernel plus programs, names, file data, and WRITE data | 53,248 bytes (52 KiB) |
| Files | 64 |
| Tasks | 16, at least one |
| Instructions per task | 256, at least one |
| Individual file | 32,768 bytes |
| Individual WRITE | 4,096 bytes |
| OS and task names | 1..31 ASCII letters, digits, `_`, `-`, `.` |
| Absolute guest filename | 2..127 bytes; same characters plus `/` |
| Banner | 0..1,024 bytes |
| Configuration source | 1 MiB |
| Host console queue | 4,096 bytes |

Filenames reject empty, `.` and `..` components, repeated/trailing slashes,
backslashes, non-ASCII characters, and duplicates. Task names must be distinct.
The compiler checks every operand and offset before publishing an image.
`mulib_oix_validate` checks the entire canonical structure, CRCs, native kernel
bytes, task initial state, file references, nonoverlapping data, and jump targets.
It rejects truncation, trailing bytes, aliased payloads and changed native code,
even when the CRCs have been recomputed. It is a structural validator, not
cryptographic authentication; valid malicious configurations can loop or print
arbitrary data. Apply caller budgets and treat console data as untrusted.

Optional `mulib_oix_info_t` is unchanged on failed validation. On success its name
borrows image bytes; the remaining fields report image/kernel sizes, counts and
RAM size. Validation does not allocate. Compilation allocates a bounded image
buffer; text compilation also uses the existing parsers, a bounded normalized
configuration, and temporary decoded data. Each image owns its allocation;
`image.length` is the serialized size, not the allocation capacity.

## VM lifecycle and errors

`mulib_oix_vm_create(bytes, length, error)` validates first, then copies the image
into private guest RAM and creates one KVM vCPU. The caller can free its image
immediately afterwards. The VM owns its KVM descriptors, shared vCPU mapping,
RAM and input queue. `mulib_oix_vm_destroy` accepts NULL and releases these; a
successfully destroyed VM pointer must not be reused. Creation unwinds partially
acquired resources on failure. No host files or devices are passed to the guest.

`mulib_oix_vm_feed` copies queued input, reports the accepted prefix via optional
`written`, and returns AGAIN for a full queue, including partial writes. NULL
bytes with nonzero length and input after halt/fault are invalid. Zero-length
input succeeds on a live VM. The queue accepts binary NUL bytes. Nothing opens
stdin, changes terminal state, or creates a thread automatically.

`mulib_oix_vm_step` processes one KVM exit into OUTPUT, TICK, INPUT, or HALTED.
OUTPUT contains one byte; INPUT reports an availability probe or consumed byte.
TICK increments once per attempted task instruction. HALTED contains the guest
status and repeats on subsequent calls without entering KVM. Unexpected I/O,
HLT before completion, MMIO, exceptions, or shutdown fault the VM permanently.
A failed step leaves its event unchanged. KVM I/O completion is committed on the
next KVM_RUN; events are host observations, not migratable vCPU snapshots.

`mulib_oix_vm_run` processes at most `exit_budget` exits. Its optional callback
receives a borrowed single-byte buffer and must return exactly 1 on success;
NULL discards output. Callback failure returns HOST_ERROR after consuming that
event. LIMIT indicates an exhausted budget and leaves the VM resumable. A valid
VM always yields cumulative result counters even on failure; counters saturate
at UINT64_MAX. A zero budget succeeds only for an already halted VM. Completion
with a nonzero guest code is OK with that code in the result, not a host error.

KVM_RUN EINTR or EAGAIN returns INTERRUPTED without automatic retry; retry from
the caller when desired. Setup ioctls retry EINTR internally. Cancel or pause by
stopping between `step` calls, using finite exit budgets, or interrupting a
blocking KVM_RUN with a caller-managed signal. The exit budget counts work, not
elapsed time; it does not promise a wall-clock deadline. An application needing
a strict deadline must arrange its own interrupt mechanism. The compiled kernel
emits frequent I/O events, including when waiting for input.

Independent configs/images/VMs may be used concurrently. All operations on one
VM, including feed and destroy, require external synchronization. Callbacks must
not reenter or destroy their VM. No mutable global runtime state is used.

Status values are OK, INVALID, FORMAT, LIMIT, MEMORY, SYSTEM, UNSUPPORTED,
INTERRUPTED, HOST_ERROR, GUEST_FAULT and AGAIN. Optional errors are initialized
per operation; the first error in that operation is retained. Messages have
static lifetime. Saved errno is supplied for system/KVM availability failures;
parser offsets and task/instruction indices identify applicable failures, with
SIZE_MAX indices otherwise. Underlying parser allocation failures may be reported
as FORMAT because parser diagnostics do not uniformly distinguish allocation.
`mulib_oix_version()` returns ABI revision 1. `mulib_oix_features()` reports
compiler/platform support; its KVM bit does not promise that `/dev/kvm` exists
or that permission or virtualization support is available.

## Image ABI revision 1

All integers are little-endian. The 64-byte header precedes a canonical payload:

| Byte offset | Field |
| --- | --- |
| 0 | Four bytes `OIX\0` |
| 4, 6 | u16 version 1, header length 64 |
| 8, 12 | u32 architecture 1 (x86 real mode), RAM 65536 |
| 16, 20, 24 | u32 load 0x1000, entry 0x1000, stack 0xfff0 |
| 28, 32 | u32 payload length, CRC32 of payload |
| 36 | u32 required feature bits 7 |
| 40, 42 | u16 file/task counts |
| 44, 46 | u16 absolute guest pointers to task/file tables |
| 48, 50, 52 | u16 name pointer, banner pointer, banner byte length |
| 54, 56 | u16 kernel length; u32 kernel ABI 1 |
| 60 | u32 CRC32 of first 60 header bytes |

Payload order: native kernel; NUL-terminated OS name; banner; file records;
per-file NUL-terminated name and exact data; task records; per-task NUL-terminated
name, instruction records and WRITE payloads. Each file record is four u16s:
name pointer, name length excluding NUL, data pointer, data length. Each 16-byte
task record has u16 PC, code base, code end, accumulator, state, name pointer,
exit status and reserved zero. PC initially equals base; accumulator/state/status
are zero. Each eight-byte instruction has u16 opcode, operand, auxiliary length,
and reserved zero. Only WRITE uses auxiliary. Jumps use resolved guest pointers,
file instructions use file indices, and WRITE uses a data pointer.

The loader zeroes 64 KiB, copies payload to 0x1000, and initializes DS/ES/SS/CS
bases to zero, RIP to 0x1000, SP to 0xfff0, RFLAGS to 2, CR0 to 0x10, CR3/CR4/EFER
to zero. The kernel starts with CLI/CLD. Kernel/data end at or below 0xe000,
leaving stack space above. Fixed low-memory u16 fields are file table at 0x500,
task table 0x502, task count 0x504, current index 0x506, live count 0x508, latched
exit code 0x50a, banner pointer/length 0x50c/0x50e, name pointer/length
0x510/0x512, file count 0x514, and diagnostic tick low word 0x518.

Port ABI: OUT byte 0xe9 writes console; IN byte 0xea probes queued input; IN byte
0xeb dequeues input or returns zero; OUT byte 0xec signals a scheduler turn;
OUT word 0xf4 signals completion with a status 0..255. The host rejects other
widths, directions, counts and ports. The kernel needs no BIOS calls or interrupts.
The KVM setup reserves the Intel TSS address when supported. Its setup and I/O
handling follow the Linux kernel KVM API documentation (`virt/kvm/api`).

## Verification

`tests/test_oix.c` executes the emitted native x86 kernel through an independent,
test-only instruction interpreter in `tests/oix_cpu.h`. It covers all task
instructions, scheduling, blocked input and binary data; it also tests format
equivalence, malformed configs, every image truncation and single-byte corruption,
CRC-valid structural attacks, allocation failures, KVM device queue handling, and simulated KVM interruption/resume and callback errors.
This interpreter is not exposed as a production backend or KVM substitute.

Actual KVM execution is an explicit hardware test:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/test_oix.c -lm -o test-oix
./test-oix
MULIB_OIX_TEST_KVM=1 ./test-oix
```

The latter requires accessible KVM and fails if unavailable; normal portable tests
clearly report the hardware test as skipped. Instruction interpretation verifies
the generated kernel behavior but cannot validate KVM setup or hardware execution.
