// repl/metaml.d — the MetaML REPL (D dialect).
//
// A thin wrapper over DSLtk.Session (DSLtk.d) instantiated with the
// MetaML.ReplLanguage policy (MetaML.d). Mirrors repl/metaml.cpp exactly.
//
//   metaml [options] [file.ml ...]
//
// Build: ldc2 -O -I.. metaml.d ../MetaML.d ../DSLtk.d [metalib modules] -of=metaml
//        add -d-version=MetaMLReadline -L-lreadline for GNU Readline support.
module metaml_repl;

import MetaML;
import DSLtk : ReplSession = Session, LineReader, render_diagnostic;
import std.stdio : stdin, stdout, stderr, write, writeln;
import std.typecons : Nullable, nullable;
import std.conv : to;

static if (__traits(compiles, { import metalib.d.Metalib; }))
{
    import metalib.d.Metalib : installAll;
    enum haveMetalib = true;
}
else
    enum haveMetalib = false;

version (MetaMLReadline)
{
    extern (C) char* readline(const(char)* prompt);
    extern (C) void add_history(const(char)* line);
    extern (C) __gshared const(char)* rl_basic_word_break_characters;
    extern (C) __gshared int rl_attempted_completion_over;
    alias rl_compentry_func_t = extern (C) char* function(const(char)*, int);
    alias rl_completion_func_t = extern (C) char** function(const(char)*, int, int);
    extern (C) __gshared rl_completion_func_t rl_attempted_completion_function;
    extern (C) char** rl_completion_matches(const(char)*, rl_compentry_func_t);
}

private struct Options
{
    bool no_tty;
    bool no_history;
    bool no_stdlib;
    bool list_primitives;
    bool interactive_after;
    string stdlib;
    string dump_ast;
    string[] disabled;
    string[] exprs;
    string[] files;
}

private enum usage_text = "usage: metaml [options] [file.ml ...]\n"
    ~ "  --no-tty          never print prompts\n"
    ~ "  --no-history      do not read or write the history file\n"
    ~ "  --no-stdlib       do not load metalib\n"
    ~ "  --stdlib DIR      use DIR as the metalib directory\n"
    ~ "  --dump-ast FILE   print the AST of FILE and exit\n"
    ~ "  --list-primitives print registered name, arity and type (TSV)\n"
    ~ "  --disable CODE    disable a warning (e.g. M0307)\n"
    ~ "  -e EXPR           evaluate EXPR as REPL input and exit\n"
    ~ "  -i                start the REPL after running files\n"
    ~ "  --version         print the version and exit\n";

private string history_path()
{
    import std.process : environment;
    import std.file : mkdirRecurse;
    string base;
    auto x = environment.get("XDG_STATE_HOME");
    if (x.length) base = x;
    else
    {
        auto h = environment.get("HOME");
        if (!h.length) return "";
        base = h ~ "/.local/state";
    }
    try mkdirRecurse(base ~ "/metaml");
    catch (Exception) {}
    return base ~ "/metaml/history";
}

private long mtime_of(string path)
{
    import std.file : timeLastModified;
    try return timeLastModified(path).stdTime;
    catch (Exception) return -1;
}

private bool isTTY()
{
    import core.sys.posix.unistd : isatty;
    return isatty(0) && isatty(1);
}

version (MetaMLReadline)
{
    private __gshared ReplSession!ReplLanguage g_repl;
    private __gshared string[] g_matches;

    extern (C) char* completion_generator(const(char)* text, int state)
    {
        import std.string : fromStringz, toStringz;
        import core.stdc.string : strdup;
        if (state == 0) g_matches = g_repl.complete(fromStringz(text).idup);
        if (state < g_matches.length) return strdup(g_matches[state].toStringz);
        return null;
    }

    extern (C) char** completion(const(char)* text, int, int)
    {
        rl_attempted_completion_over = 1;
        return rl_completion_matches(text, &completion_generator);
    }
}

private int run(string[] argv)
{
    import core.stdc.stdlib : exit, atoi;
    import std.process : environment;
    Options opt;
    for (size_t i = 1; i < argv.length; ++i)
    {
        string a = argv[i];
        string need()
        {
            if (i + 1 >= argv.length)
            {
                stderr.writeln("metaml: option " ~ a ~ " needs an argument");
                exit(2);
            }
            return argv[++i];
        }
        if (a == "--no-tty") opt.no_tty = true;
        else if (a == "--no-history") opt.no_history = true;
        else if (a == "--no-stdlib") opt.no_stdlib = true;
        else if (a == "--list-primitives") opt.list_primitives = true;
        else if (a == "--stdlib") opt.stdlib = need();
        else if (a == "--dump-ast") opt.dump_ast = need();
        else if (a == "--disable") opt.disabled ~= need();
        else if (a == "-e") opt.exprs ~= need();
        else if (a == "-i") opt.interactive_after = true;
        else if (a == "--version")
        {
            writeln("MetaML " ~ metamlVersion ~ " (" ~ dialect ~ ")");
            return 0;
        }
        else if (a == "--help" || a == "-h")
        {
            write(usage_text);
            return 0;
        }
        else if (a.length && a[0] == '-')
        {
            stderr.write("metaml: unknown option " ~ a ~ "\n" ~ usage_text);
            return 2;
        }
        else opt.files ~= a;
    }

    if (opt.dump_ast.length)
    {
        import DSLtk : load_parse_input_file;
        auto text = load_parse_input_file(opt.dump_ast);
        if (text.is_err())
        {
            stderr.writeln("metaml: cannot read " ~ opt.dump_ast);
            return 1;
        }
        auto parsed = parse(text.unwrap(), opt.dump_ast);
        writeln(dump_ast(parsed.program));
        foreach (d; parsed.diagnostics) write(render_diagnostic(d, text.unwrap()));
        return parsed.ok() ? 0 : 1;
    }

    auto session = new MetaML.Session();
    scope (exit) session.close();
    session.allow_all_primitives();
    session.set_max_depth(100000);
    foreach (c; opt.disabled) session.disable_warning(c);
    auto td = environment.get("METAML_TRACE_DEPTH");
    if (td.length) session.set_trace_depth(atoi((td ~ "\0").ptr));
    static if (haveMetalib) installAll(session);
    if (!opt.no_stdlib)
    {
        auto r = session.load_stdlib(opt.stdlib);
        if (r.is_err())
        {
            foreach (d; r.unwrap_err()) stderr.write(session.render(d));
            return 1;
        }
    }

    if (opt.list_primitives)
    {
        foreach (name; session.primitive_names())
        {
            auto p = session.find_primitive(name);
            writeln(name, "\t", p.arity, "\t", p.type_src);
        }
        return 0;
    }
    int status = 0;
    foreach (f; opt.files)
    {
        auto r = session.run_file(f, RunMode.Quiet);
        stdout.write(r.text);
        stdout.flush();
        foreach (w; r.warnings) stderr.write(session.render(w));
        foreach (d; r.errors)
        {
            stdout.flush();
            stderr.write(session.render(d));
            stderr.flush();
        }
        if (!r.ok())
        {
            status = 1;
            break;
        }
    }

    auto lang = new ReplLanguage(session);
    auto repl = new ReplSession!ReplLanguage(lang, (string s) { stdout.write(s); });
    repl.set_prompts("meaml> ", ".. > ");
    install_repl_directives(repl, session);

    if (opt.exprs.length)
    {
        foreach (e; opt.exprs)
        {
            repl.feed_multiline(e);
            repl.finish();
        }
        stdout.flush();
        return repl.error_count() ? 1 : status;
    }
    if (opt.files.length && !opt.interactive_after) return status;

    bool tty = !opt.no_tty && isTTY();
    string hist = (!opt.no_history && tty) ? history_path() : "";
    if (hist.length) repl.load_history(hist);

    // live reload (§18.5): re-load the last #load-ed file when it changes
    string watched;
    long watched_time = -1;
    void check_reload()
    {
        string last = session.last_loaded();
        if (!last.length) return;
        long t = mtime_of(last);
        if (watched != last)
        {
            watched = last;
            watched_time = t;
            return;
        }
        if (t != watched_time && t >= 0)
        {
            watched_time = t;
            auto r = session.directive("reload", "");
            stdout.write("(reloaded " ~ last ~ ")\n" ~ r.text);
            foreach (d; r.errors) stdout.write(session.render(d));
        }
    }

    LineReader reader;
    version (MetaMLReadline)
    {
        if (tty)
        {
            import std.string : toStringz, fromStringz;
            import core.stdc.stdlib : free;
            g_repl = repl;
            rl_attempted_completion_function = &completion;
            rl_basic_word_break_characters = " \t\n\"\\'`@$><=;|&{(,[".ptr;
            foreach (h; repl.history()) add_history(h.toStringz);
            reader = (string prompt) {
                check_reload();
                char* line = readline(prompt.toStringz);
                if (line is null) return Nullable!string.init;
                string s = fromStringz(line).idup;
                free(line);
                if (s.length) add_history(s.toStringz);
                return nullable(s);
            };
        }
    }
    if (reader is null && tty)
        reader = (string prompt) {
            check_reload();
            stdout.write(prompt);
            stdout.flush();
            auto l = stdin.readln();
            if (l is null) return Nullable!string.init;
            if (l.length && l[$ - 1] == '\n') l = l[0 .. $ - 1];
            return nullable(l);
        };
    if (tty)
        writeln("MetaML " ~ metamlVersion ~ " (" ~ dialect ~ ") — #help for directives, #quit to leave");
    repl.run(() {
        auto l = stdin.readln();
        if (l is null) return Nullable!string.init;
        if (l.length && l[$ - 1] == '\n') l = l[0 .. $ - 1];
        return nullable(l);
    }, tty, reader);
    if (hist.length) repl.save_history(hist);
    if (tty) writeln();
    stdout.flush();
    return repl.error_count() ? 1 : status;
}

int main(string[] argv)
{
    import core.thread : Thread;
    // Run on a thread with a large stack so deep (non-tail) recursion works.
    int status = 0;
    auto t = new Thread(() { status = run(argv); }, cast(size_t) 1 << 30);
    t.start();
    t.join();
    stdout.flush();
    return status;
}
