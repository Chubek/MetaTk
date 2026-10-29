# MuLIB minish

**Header:** `include/mulib-minish.h`  
**Implementation switches:** `MULIB_IMPL_MINISH`, `MULIB_IMPL_MAP`, `MULIB_IMPL_VEC`, `MULIB_IMPL_STRBUF`  
**Dependencies:** MuLIB map/vec/strbuf and ISO C11. Optional POSIX OS APIs only.

## Scope and grammar

minish is a small command interpreter, **not a POSIX shell**. It tokenizes real
quoted/escaped words, expands variables, runs builtins and registered C
callbacks, and optionally launches exact-path executables without passing input
to `/bin/sh`, `system`, or `popen`.

Supported grammar:

* Unquoted spaces/tabs/CR/vertical-tab/form-feed separate words. Semicolons and
  newlines separate commands; empty commands are ignored.
* Single quotes preserve every byte other than the closing quote.
* Double quotes preserve whitespace/separators, allow variable expansion, and
  allow backslash escapes. Adjacent quoted/unquoted pieces form **one** word.
* Backslash outside single quotes quotes **any** next byte. Backslash-newline
  removes both bytes. This simplified double-quote escape rule deliberately
  differs from POSIX shells. A trailing backslash or unterminated quote is an error.
* `#` begins a comment only at word start, continuing to newline. Within a word
  or quotes it is literal.
* `$NAME`, `${NAME}`, and `$?` expand interpreter variables and the previous
  command status. Names are ASCII `[A-Za-z_][A-Za-z0-9_]*`, up to 128 bytes.
  Undefined names produce an empty string. An empty expansion still contributes
  a word. Expansion never performs field splitting, globbing, recursive
  expansion, or syntax evaluation, even when its value contains separators.

Unsupported: pipes, `&&`/`||`/background `&`, redirects, grouping, command or
arithmetic substitution, backticks, positional/special parameters other than
`$?`, `${name:-default}` and other parameter operators, assignment statements,
loops, conditionals, functions, aliases, tilde expansion, and glob expansion.
Unquoted `| & < > ( )`, backticks outside single quotes, `$(`, `$$`, `$1`, etc.
are rejected as `UNSUPPORTED`, not silently executed differently. Single-quoted
or backslash-escaped metacharacters are literal; double quotes make operator
characters literal but still reject unsupported substitutions. Globs, `~`, and ordinary `=` characters are
literal bytes; e.g. `NAME=value` is a command name, **not** an assignment.
There is no environment-variable fallback, PATH search, export, or `cd` builtin.
Variable values are arbitrary NUL-terminated byte strings, not Unicode-normalized.
Source has an explicit byte length; embedded NUL anywhere is rejected up front.

## Lifecycle and configuration API

```c
mulib_minish_limits_t mulib_minish_default_limits(void);
void mulib_minish_init(mulib_minish_t *);
void mulib_minish_free(mulib_minish_t *);
mulib_minish_status_t mulib_minish_set_limits(mulib_minish_t *, const mulib_minish_limits_t *);
void mulib_minish_set_writer(mulib_minish_t *, mulib_minish_write_fn, void *user);
mulib_minish_status_t mulib_minish_allow_process(mulib_minish_t *, int enabled);
```

Initialize before use; do not copy the owning struct. Initialization allocates
nothing. `free` releases copied variable values/keys and command names, not
callback user data. It leaves the object in default initialized state. No API
implicitly frees the interpreter, and no function is thread-safe for a shared
mutable interpreter. Do not free/reinitialize it from a callback.

Limits have fields `input_bytes`, `word_bytes`, `words`, `commands`, `variables`,
and `registered`. Defaults are 65536 input bytes per execute, 16384 expanded
bytes per word, 256 words per command, 128 commands per execute, 256 persistent
variables, and 128 registered callbacks. Explicit zero means zero. Changing
limits is allowed only while idle; shrinking below existing variable values,
variable counts, or registration counts is rejected. Impossible allocation
sizes are rejected. New/updated values and every appended token fragment are
checked; allocation multiplication is checked by the containers. The bounds do
not cap the caller's output capture or arbitrary native callback allocations.

The writer has signature `int(void *user, const char *text, size_t length)`;
return 1 only after accepting all bytes. It is borrowed; NULL restores stdout
via `fwrite`. Builtin echo uses it; a failed write is `IO_ERROR`. Output can
already be partially written when failure occurs.

## Variables and command dispatch API

```c
mulib_minish_status_t mulib_minish_set(mulib_minish_t *, const char *name, const char *value);
const char *mulib_minish_get(const mulib_minish_t *, const char *name);
mulib_minish_status_t mulib_minish_unset(mulib_minish_t *, const char *name);
mulib_minish_status_t mulib_minish_register(mulib_minish_t *, const char *name,
                                          mulib_minish_command_fn, void *user);
int mulib_minish_unregister(mulib_minish_t *, const char *name);
mulib_minish_status_t mulib_minish_execute(mulib_minish_t *, const char *source,
                                         size_t length, mulib_minish_error_t *);
const char *mulib_minish_status_string(mulib_minish_status_t);
```

`set` copies both name and value; replacement is safe even if `value` points to
the old value. `get` returns a borrowed pointer until replacement/unset/free,
or NULL for an absent/invalid name. `unset` succeeds when already absent.
Host-registered command names use the variable-name grammar but additionally
allow `-` after the first byte; builtin names are reserved. Registering an
existing name replaces its callback/user without adding another entry.
Unregister returns 1 if removed and 0 if absent/invalid.

A command callback is:

```c
int command(mulib_minish_t *shell, size_t argc,
            const char *const *argv, void *user);
```

`argc` includes the command name at `argv[0]`; argv is count-based, **not**
promised NULL-terminated for callbacks. Strings/array are borrowed only during
the call. Return a command status in 0..255; other values are `CALLBACK_ERROR`.
Callbacks may set/unset variables and register/unregister commands, but must not
free the shell, retain argv, modify internals, or recursively execute on that
same shell. Recursive execute is explicitly rejected. Callback user data is
never copied or freed. Builtins are dispatched before callbacks, then optional
processes; otherwise an unknown command is `NOT_FOUND`.

## Builtins and command status

| Command | Behavior |
|---|---|
| `echo [words...]` | Join words with spaces and write newline; no options/escape processing |
| `set NAME VALUE` | Copy one variable value |
| `unset NAME` | Remove one variable |
| `true` / `false` | Status 0 / 1, no arguments |
| `exit [STATUS]` | Request exit; decimal 0..255, default previous status |

Wrong arity, invalid builtin variable names, and invalid exit status produce
command status 2, not an engine error. Allocation/resource failures still stop
execution with an engine error. A nonzero command status alone does not stop
the script. `shell.last_status` changes only after a successfully dispatched
command and supplies `$?`. Engine errors leave it unchanged.

Exit sets `exit_requested`/`exit_status`, stops later commands, and makes future
execute calls no-ops until the caller explicitly clears `exit_requested`.
Other interpreter internals should not be modified directly.

## Errors and sequencing

`MULIB_MINISH_OK` means tokenization/dispatch succeeded, **not** that every
command returned zero. Other statuses are `INVALID_ARGUMENT`, `NO_MEMORY`,
`SYNTAX`, `LIMIT`, `INVALID_NAME`, `NOT_FOUND`, `CALLBACK_ERROR`, `UNSUPPORTED`,
`IO_ERROR`, and `PROCESS_ERROR`, all prefixed `MULIB_MINISH_`.

The optional error output is always initialized and contains `status`, byte
`offset`, zero-based `command`, and `system_error` (OS errno only for process
errors; otherwise zero). Syntax/expansion/token limits identify the offending
byte or opening quote; dispatch errors identify the command's first word.
On success offset equals source length and command equals commands executed.
NULL source is valid only for length zero. Status text is static; unknown codes
get a fallback. No general `errno` contract is provided.

Commands are tokenized, expanded, and dispatched one at a time, so a preceding
`set` affects expansion in the next command. A malformed later command leaves
earlier side effects intact. Input-length and embedded-NUL checks occur before
any command. Execution is not transactional.

## Opt-in POSIX processes

Define `MULIB_MINISH_ENABLE_POSIX` in the **implementation** translation unit
and call `mulib_minish_allow_process(&shell, 1)` while idle. Without the build
option, enabling returns `UNSUPPORTED`; without the per-shell opt-in, unknown
commands remain `NOT_FOUND`.

The implementation uses fork, **execv** (not execvp), and waitpid with a
close-on-exec pipe for precise launch errno. The first argument is an exact
absolute or relative executable path, with **no PATH lookup or shell fallback**.
Every remaining expanded word is passed as one argv element. Kernel shebang
handling still works, but a non-executable/non-shebang script yields a launch
error instead of being interpreted as shell text. A caller can explicitly opt
to run a shell executable; minish never inserts one.

Processes inherit the host environment, current directory, and standard
descriptors; minish variables are not exported. Output bypasses the builtin
writer. Normal exit codes become command statuses; signals become 128+signal
(capped at 255). Launch/wait errors are `PROCESS_ERROR` with `system_error`.
Processes are synchronous and have no time limit. This facility is not a
sandbox and should remain disabled for untrusted commands.

## Runnable C example

```c
#define MULIB_IMPL_MAP
#define MULIB_IMPL_VEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_MINISH
#include "mulib-minish.h"
#include <assert.h>
#include <string.h>
static int capture(void *user, const char *text, size_t n) {
    return mulib_strbuf_append_n((mulib_strbuf_t *)user, text, n);
}
int main(void) {
    const char script[] = "set who 'MuLIB user'; echo \"Hello, ${who}\"";
    mulib_minish_t shell;
    mulib_strbuf_t output;
    mulib_minish_init(&shell);
    mulib_strbuf_init(&output);
    mulib_minish_set_writer(&shell, capture, &output);
    assert(mulib_minish_execute(&shell, script, sizeof(script)-1, NULL) == MULIB_MINISH_OK);
    assert(!strcmp(output.data, "Hello, MuLIB user\n"));
    mulib_minish_free(&shell);
    mulib_strbuf_free(&output);
    return 0;
}
```

Build with `cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude example.c -o example`.
The example needs no shell/interpreter or process support. The repository's
`examples/modules/minish.c` also demonstrates a registered command.

`tests/test_runtime.c` covers quoting/concatenation, empty words, comments,
expansion/injection resistance, variable ownership, malformed grammar, exact
offsets, limits, writer/callback failures, exit handling, and recursion rejection.
Build it with the same flags and optionally ASan+UBSan. Adding
`-DMULIB_MINISH_ENABLE_POSIX` also checks direct argv passing to a child copy of
the test binary and precise ENOENT reporting. All implementation switches must
be defined explicitly before includes; no dependency switch is enabled by the
header. Declaration-only C++17 callers are supported via C linkage.
