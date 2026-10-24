import MetaML;
import metalib.d.Metalib;
import std.stdio : File;
import std.file : tempDir, remove, isDir, exists;
import std.path : buildPath;

void main(string[] args)
{
    assert(args.length == 2);
    foreach (source; ["$999999999999999999999999", "(#999999999999999999999999 + 1)"])
        assert(lex(source).diagnostics.length);
    foreach (text; ["", "_", "1_", "1__2", "0x_1"])
        assert(parse_int(text).isNull);
    foreach (text; ["", "_", "\v1", "1_", "1__2"])
        assert(parse_float(text).isNull);
    assert(parse_int("1_000").get == 1000);
    assert(parse_float("1_000.5").get == 1000.5);
    foreach (source; ["1_", "1.0__2", "1__0"])
        assert(lex(source).diagnostics.length);
    auto s = new Session;
    assert(s.load_stdlib(args[1]).is_ok());
    assert(s.eval("List.range (~9223372036854775808) 9223372036854775807").is_err());
    assert(s.eval("[Int.min_value .. Int.max_value]").is_err());
    assert(to_string(s.eval("[Int.max_value .. Int.max_value]")) == "[9223372036854775807]");
    assert(s.eval("String.repeat 9223372036854775807 \"\"").is_ok());
    assert(s.eval("String.repeat 9223372036854775807 \"xx\"").is_err());
    s.register_primitive("Host.boom", (long n) { throw new Exception("host failure"); return n; });
    auto bad = s.eval("Host.boom 0");
    assert(bad.is_err() && bad.unwrap_err()[0].code == "M0409");
    assert(to_string(s.eval("1 + 2")) == "3");
    s.register_primitive("Host.inc", (long n) => n + 1);
    assert(s.eval("saved := Host.inc").is_ok());
    s.register_primitive("Host.inc", (long n) => n + 2);
    assert(to_string(s.eval("saved 1")) == "2");
    assert(to_string(s.eval("Host.inc 1")) == "3");
    bool rejected = false;
    try s.install_primitive("Host.inc", "Int -> Int", 1, null);
    catch (Exception) { rejected = true; }
    assert(rejected && to_string(s.eval("Host.inc 1")) == "3");
    import std.path : dirName;
    s.set_output((string text) {});
    assert(s.load_file(buildPath(dirName(args[1]), "tests/integration/extensions.ml")).is_ok());
    import std.uuid : randomUUID;
    auto temporary = exists("/tmp/opencode") && isDir("/tmp/opencode") ? "/tmp/opencode" : tempDir();
    auto file = buildPath(temporary, "metaml-retry-d-" ~ randomUUID().toString() ~ ".ml");
    { auto f = File(file, "w"); f.writeln("val retry := missing_name"); }
    auto source = "import " ~ quote_string(file);
    assert(s.eval(source).is_err());
    { auto f = File(file, "w"); f.writeln("val retry := 42"); }
    assert(s.eval(source).is_ok());
    assert(to_string(s.eval("retry")) == "42");
    remove(file);

    import std.file : mkdirRecurse, rmdirRecurse;
    auto broken = file ~ ".stdlib";
    mkdirRecurse(buildPath(broken, "prelude"));
    { auto f = File(buildPath(broken, "prelude/mod.ml"), "w");
      f.writeln("val broken := missing_name"); }
    auto original = s.stdlib_dir();
    assert(s.load_stdlib(broken).is_err());
    assert(s.stdlib_dir() == original && s.stdlib_loaded());
    rmdirRecurse(broken);
    assert(to_string(s.eval("1 + 2")) == "3");

    installFoundation(s);
    assert(s.find_primitive("JSON.parse") is null && s.find_primitive("IO.flush") is null);
    s.allow_all_primitives();
    assert(s.eval("buffer := Buffer.create ()").is_ok());
    auto create = s.find_primitive("Buffer.create");
    installData(s);
    installAll(s);
    auto names = s.primitive_names();
    installAll(s);
    assert(names == s.primitive_names() && create is s.find_primitive("Buffer.create"));
    assert(s.eval("Buffer.add \"kept\" buffer").is_ok());
    assert(to_string(s.eval("Buffer.contents buffer")) == "\"kept\"");
}
