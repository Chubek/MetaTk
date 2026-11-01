# mulib-termutils

POSIX terminal mode, window-size and complete-write helpers. Complements
`mulib-termkey`/`mulib-terminfo`; no curses dependency, escape-sequence renderer
or process-global cleanup hooks.

## Integration and ownership

Define `MULIB_IMPL_TERMUTILS` in one C11 translation unit. Native operations work
on Unix/macOS and return ENOTSUP elsewhere. Initialize raw state with
`MULIB_TERMUTILS_RAW_INIT`; do not copy it. `raw_begin(&state,fd)` saves native
attributes in owned heap storage, then sets raw 8-bit mode with VMIN=1, VTIME=0.
The fd is borrowed: keep it open and referring to the same terminal until restore.

`raw_end(&state)` restores attributes, releases saved storage and resets state.
It is idempotent and retains saved state after native failure for retry. Restore
on every normal/error path; abrupt termination/signals require application-level
cleanup. Do not stack independent raw states on the same fd or change terminal
attributes concurrently. No `atexit`, signal-handler installation or fd ownership.

## Results and writes

Success zero; EINVAL=-1, EIO=-2, ENOTSUP=-3, ESTATE=-4, NOMEM=-5. EIO preserves
errno. `size(fd,&columns,&rows)` queries TIOCGWINSZ; native zero dimensions remain
zero (no guessed defaults). Outputs unchanged on error.

`write(fd,bytes,length,&written)` retries EINTR and short writes, using bounded
chunks, until complete. On EIO it reports partial progress; EAGAIN remains in
errno and callers may resume from `written`. Zero-length data may be NULL. No
SIGPIPE suppression: callers writing to pipes must select their own signal policy.

`cc -std=c11 -Iinclude examples/modules/termutils.c -o termutils` does not alter
the user's console. Tests use an isolated pseudo-terminal for raw/restore/size
and a pipe for complete writes.
