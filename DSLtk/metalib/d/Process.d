/// POSIX process operations with separate stdout/stderr capture.
module metalib.d.Process;
import metalib.d.Support;
import core.stdc.errno;
import core.stdc.string : strerror;
import core.stdc.stdlib : getenv, _Exit;
import core.sys.posix.stdlib : setenv;
import core.stdc.stdio : fflush;
import core.sys.posix.unistd;
import core.sys.posix.fcntl;
import core.sys.posix.poll;
import core.sys.posix.spawn;
import core.sys.posix.sys.wait;
import core.sys.posix.time;
import std.string : toStringz, fromStringz;

private struct Captured { int status = -1; string output; string errors; string error; }
private Captured capture(string[] argv, bool captureError)
{
    Captured result;
    int[2] po, pe = [-1, -1];
    if (pipe(po) != 0) { result.error = fromStringz(strerror(errno)).idup; return result; }
    if (captureError && pipe(pe) != 0)
    {
        result.error = fromStringz(strerror(errno)).idup;
        close(po[0]); close(po[1]); return result;
    }
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&fa, po[1], 1);
    posix_spawn_file_actions_addclose(&fa, po[0]);
    posix_spawn_file_actions_addclose(&fa, po[1]);
    if (captureError)
    {
        posix_spawn_file_actions_adddup2(&fa, pe[1], 2);
        posix_spawn_file_actions_addclose(&fa, pe[0]);
        posix_spawn_file_actions_addclose(&fa, pe[1]);
    }
    const(char)*[] args;
    foreach (a; argv) args ~= a.toStringz;
    args ~= null;
    pid_t pid;
    int rc = posix_spawnp(&pid, args[0], &fa, null, args.ptr, environ);
    posix_spawn_file_actions_destroy(&fa);
    close(po[1]); if (captureError) close(pe[1]);
    if (rc)
    {
        close(po[0]); if (captureError) close(pe[0]);
        result.error = fromStringz(strerror(rc)).idup; return result;
    }
    pollfd[2] fds = [pollfd(po[0], POLLIN, 0), pollfd(pe[0], POLLIN, 0)];
    int openFds = captureError ? 2 : 1;
    char[65536] buffer;
    while (openFds)
    {
        if (poll(fds.ptr, captureError ? 2 : 1, -1) < 0)
        { if (errno == EINTR) continue; break; }
        foreach (k; 0 .. captureError ? 2 : 1)
        {
            if (fds[k].fd < 0 || !(fds[k].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            auto n = read(fds[k].fd, buffer.ptr, buffer.length);
            if (n > 0)
            {
                if (k == 0) result.output ~= buffer[0 .. n];
                else result.errors ~= buffer[0 .. n];
            }
            else if (n == 0 || errno != EINTR) { close(fds[k].fd); fds[k].fd = -1; --openFds; }
        }
    }
    foreach (ref fd; fds) if (fd.fd >= 0) close(fd.fd);
    int status;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    result.status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    return result;
}

void installProcess(Session s)
{
    prim(s, "Process.run", "String -> [String] -> Result (Int, String, String) String", 2, (C c, V a) {
        string[] argv = [S(a[0])]; foreach (v; items(a[1])) argv ~= S(v);
        auto r = capture(argv, true);
        return r.error.length ? err("Process.run: cannot run `" ~ S(a[0]) ~ "`: " ~ r.error)
            : ok(triple(integer(r.status), str(r.output), str(r.errors)));
    });
    prim(s, "Process.shell", "String -> Result (Int, String) String", 1, (C c, V a) {
        c.session.flush_output();
        auto r = capture(["/bin/sh", "-c", S(a[0])], false);
        return r.error.length ? err("Process.shell: " ~ r.error) : ok(pair(integer(r.status), str(r.output)));
    });
    prim(s, "Process.env", "String -> Option String", 1, (C c, V a) {
        auto v = getenv(S(a[0]).toStringz); return v is null ? none() : some(str(fromStringz(v).idup));
    });
    prim(s, "Process.set_env", "String -> String -> Unit", 2, (C c, V a) {
        setenv(S(a[0]).toStringz, S(a[1]).toStringz, 1); return unit();
    });
    prim(s, "Process.args", "Unit -> [String]", 1, (C c, V a) => strings(c.session.args));
    prim(s, "Process.cwd", "Unit -> String", 1, (C c, V a) {
        char[4096] buf; return str(getcwd(buf.ptr, buf.length) is null ? "" : fromStringz(buf.ptr).idup);
    });
    prim(s, "Process.chdir", "String -> Result Unit String", 1, (C c, V a) {
        return chdir(S(a[0]).toStringz) == 0 ? ok(unit())
            : err("Process.chdir: " ~ fromStringz(strerror(errno)).idup ~ " (`" ~ S(a[0]) ~ "`)");
    });
    prim(s, "Process.pid", "Unit -> Int", 1, (C c, V a) => integer(getpid()));
    prim(s, "Process.clock_ms", "Unit -> Int", 1, (C c, V a) {
        timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return integer(ts.tv_sec * 1000 + ts.tv_nsec / 1_000_000);
    });
    prim(s, "Process.time", "Unit -> Float", 1, (C c, V a) {
        timespec ts; clock_gettime(CLOCK_REALTIME, &ts); return floating(ts.tv_sec + ts.tv_nsec / 1e9);
    });
    prim(s, "Process.exit", "Int -> 'a", 1, delegate Value(C c, V a) {
        c.session.flush_output(); fflush(null); _Exit(cast(int) a[0].i); return unit();
    });
}
