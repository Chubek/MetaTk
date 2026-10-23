/// POSIX file operations, mirrored by metalib/cpp/File.h.
module metalib.d.File;
import metalib.d.Support;
import core.stdc.errno;
import core.stdc.string : strerror;
import core.stdc.stdio : rename;
import core.stdc.stdlib : getenv;
import core.sys.posix.fcntl;
import core.sys.posix.unistd;
import core.sys.posix.sys.stat;
import core.sys.posix.dirent;
import std.string : toStringz, fromStringz, lastIndexOf, indexOf;
import std.algorithm : sort;

private Value failure(string name, string path)
{
    return err(name ~ ": " ~ fromStringz(strerror(errno)).idup ~ " (`" ~ path ~ "`)");
}
private bool readAll(string path, ref string output)
{
    int fd = open(path.toStringz, O_RDONLY);
    if (fd < 0) return false;
    stat_t st;
    if (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode))
    { close(fd); errno = EISDIR; return false; }
    char[] data;
    char[65536] buffer;
    while (true)
    {
        auto n = read(fd, buffer.ptr, buffer.length);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            int e = errno; close(fd); errno = e; return false;
        }
        if (!n) break;
        data ~= buffer[0 .. cast(size_t) n];
    }
    close(fd);
    output = data.idup;
    return true;
}
private bool writeAll(string path, string data, bool append)
{
    int fd = open(path.toStringz, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 420);
    if (fd < 0) return false;
    size_t offset;
    while (offset < data.length)
    {
        auto n = write(fd, data.ptr + offset, data.length - offset);
        if (n <= 0)
        {
            if (n < 0 && errno == EINTR) continue;
            int e = n == 0 ? EIO : errno; close(fd); errno = e; return false;
        }
        offset += n;
    }
    return close(fd) == 0;
}
private string strip(string p)
{
    while (p.length > 1 && p[$ - 1] == '/') p = p[0 .. $ - 1];
    return p;
}
private string baseName(string path)
{
    auto p = strip(path);
    if (p == "/") return p;
    auto k = p.lastIndexOf('/');
    return k < 0 ? p : p[k + 1 .. $];
}
private string dirName(string path)
{
    auto p = strip(path);
    auto k = p.lastIndexOf('/');
    return k < 0 ? "." : k == 0 ? "/" : strip(p[0 .. k]);
}
private bool mkdirs(string path)
{
    if (!path.length) { errno = ENOENT; return false; }
    size_t pos;
    while (true)
    {
        auto k = path.indexOf('/', pos + 1);
        auto part = k < 0 ? path : path[0 .. k];
        if (part.length && mkdir(part.toStringz, 493) != 0 && errno != EEXIST) return false;
        if (k < 0) break;
        pos = k;
    }
    stat_t st;
    if (stat(path.toStringz, &st) != 0) return false;
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return false; }
    return true;
}

void installFile(Session s)
{
    prim(s, "File.read", "String -> Result String String", 1, (C c, V a) {
        string output;
        return readAll(S(a[0]), output) ? ok(str(output)) : failure("File.read", S(a[0]));
    });
    prim(s, "File.read_lines", "String -> Result [String] String", 1, (C c, V a) {
        string output;
        if (!readAll(S(a[0]), output)) return failure("File.read_lines", S(a[0]));
        Value[] lines;
        size_t b;
        while (b < output.length)
        {
            auto e = output.indexOf('\n', b);
            if (e < 0) e = output.length;
            auto line = output[b .. e];
            if (line.length && line[$ - 1] == '\r') line = line[0 .. $ - 1];
            lines ~= str(line);
            b = e + 1;
        }
        return ok(list(lines));
    });
    prim(s, "File.write", "String -> String -> Result Unit String", 2,
        (C c, V a) => writeAll(S(a[0]), S(a[1]), false) ? ok(unit()) : failure("File.write", S(a[0])));
    prim(s, "File.append", "String -> String -> Result Unit String", 2,
        (C c, V a) => writeAll(S(a[0]), S(a[1]), true) ? ok(unit()) : failure("File.append", S(a[0])));
    prim(s, "File.exists", "String -> Bool", 1, (C c, V a) { stat_t st; return boolean(stat(S(a[0]).toStringz, &st) == 0); });
    prim(s, "File.is_dir", "String -> Bool", 1, (C c, V a) { stat_t st; return boolean(stat(S(a[0]).toStringz, &st) == 0 && S_ISDIR(st.st_mode)); });
    prim(s, "File.is_file", "String -> Bool", 1, (C c, V a) { stat_t st; return boolean(stat(S(a[0]).toStringz, &st) == 0 && S_ISREG(st.st_mode)); });
    prim(s, "File.size", "String -> Result Int String", 1, (C c, V a) {
        stat_t st;
        return stat(S(a[0]).toStringz, &st) == 0 ? ok(integer(st.st_size)) : failure("File.size", S(a[0]));
    });
    prim(s, "File.remove", "String -> Result Unit String", 1, (C c, V a) {
        stat_t st;
        auto p = S(a[0]).toStringz;
        if (lstat(p, &st) != 0 || (S_ISDIR(st.st_mode) ? rmdir(p) : unlink(p)) != 0)
            return failure("File.remove", S(a[0]));
        return ok(unit());
    });
    prim(s, "File.rename", "String -> String -> Result Unit String", 2,
        (C c, V a) => rename(S(a[0]).toStringz, S(a[1]).toStringz) == 0 ? ok(unit()) : failure("File.rename", S(a[0])));
    prim(s, "File.copy", "String -> String -> Result Unit String", 2, (C c, V a) {
        string data;
        if (!readAll(S(a[0]), data)) return failure("File.copy", S(a[0]));
        return writeAll(S(a[1]), data, false) ? ok(unit()) : failure("File.copy", S(a[1]));
    });
    prim(s, "File.mkdir", "String -> Result Unit String", 1,
        (C c, V a) => mkdirs(S(a[0])) ? ok(unit()) : failure("File.mkdir", S(a[0])));
    prim(s, "File.list_dir", "String -> Result [String] String", 1, (C c, V a) {
        auto d = opendir(S(a[0]).toStringz);
        if (d is null) return failure("File.list_dir", S(a[0]));
        scope (exit) closedir(d);
        string[] names;
        while (true)
        {
            errno = 0;
            auto e = readdir(d);
            if (e is null) { if (errno) return failure("File.list_dir", S(a[0])); break; }
            auto name = fromStringz(e.d_name.ptr).idup;
            if (name != "." && name != "..") names ~= name;
        }
        names.sort();
        return ok(strings(names));
    });
    prim(s, "File.basename", "String -> String", 1, (C c, V a) => str(baseName(S(a[0]))));
    prim(s, "File.dirname", "String -> String", 1, (C c, V a) => str(dirName(S(a[0]))));
    prim(s, "File.extension", "String -> String", 1, (C c, V a) {
        auto b = baseName(S(a[0])); auto k = b.lastIndexOf('.');
        return str(k <= 0 ? "" : b[k .. $]);
    });
    prim(s, "File.join", "String -> String -> String", 2, (C c, V a) {
        auto x = S(a[0]), y = S(a[1]);
        return str(!x.length || (y.length && y[0] == '/') ? y : x ~ (x[$ - 1] == '/' ? "" : "/") ~ y);
    });
    prim(s, "File.temp_dir", "Unit -> String", 1, (C c, V a) {
        auto p = getenv("TMPDIR");
        return str(p !is null && *p ? strip(fromStringz(p).idup) : "/tmp");
    });
}
