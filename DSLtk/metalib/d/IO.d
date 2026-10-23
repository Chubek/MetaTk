/**
 * metalib/d/IO.d — MetaML host module `IO` (D dialect): console helpers beyond the core IO primitives.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/IO.h):
 *   IO.flush, read_all, read_lines, printf, print_lines, is_tty
 */
module metalib.d.IO;

import MetaML;
import metalib.d.Support;


void installIO(Session s)
{
    prim(s, "IO.flush", "Unit -> Unit", 1, delegate Value(C c, V a) {
        import std.stdio : stdout;
        stdout.flush();
        return unit();
    });
    prim(s, "IO.read_all", "Unit -> String", 1, delegate Value(C c, V a) {
        import std.stdio : stdin;
        string result;
        while (true)
        {
            auto l = stdin.readln();
            if (l is null) break;
            if (l.length && l[$ - 1] == '\n') l = l[0 .. $ - 1];
            result ~= l ~ "\n";
        }
        return str(result);
    });
    prim(s, "IO.read_lines", "Unit -> [String]", 1, delegate Value(C c, V a) {
        import std.stdio : stdin;
        Value[] result;
        while (true)
        {
            auto l = stdin.readln();
            if (l is null) break;
            if (l.length && l[$ - 1] == '\n') l = l[0 .. $ - 1];
            if (l.length && l[$ - 1] == '\r') l = l[0 .. $ - 1];
            result ~= str(l);
        }
        return list(result);
    });
    prim(s, "IO.printf", "String -> [String] -> Unit", 2, delegate Value(C c, V a) {
        auto fmt = c.session.find_primitive("String.format");
        string text = S(a[0]);
        if (fmt !is null) text = S(c.apply(c.interp.prim_value(fmt), [a[0], a[1]]));
        c.print(text);
        return unit();
    });
    prim(s, "IO.print_lines", "[String] -> Unit", 1, delegate Value(C c, V a) {
        foreach (x; items(a[0])) c.print(S(x) ~ "\n");
        return unit();
    });
    prim(s, "IO.is_tty", "Unit -> Bool", 1, delegate Value(C c, V a) {
        import core.sys.posix.unistd : isatty;
        return boolean(isatty(1) != 0);
    });
}

