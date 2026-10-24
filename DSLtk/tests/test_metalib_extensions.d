import MetaML;
import metalib.d.Metalib;
import metalib.http.HTTP;
import metalib.uri.URI;
import metalib.ffi.FFI;
import metalib.parsec.Parsec;
import metalib.memory.Memory;
import metalib.shell.Shell;
import metalib.jsonrpc.JSONRPC;
import std.stdio;

int main(string[] args) {
    assert(args.length == 2);
    auto s = new Session;
    installAll(s);
    installHTTP(s);
    installFFI(s);
    installShell(s);
    installJSONRPC(s);
    s.allow_all_primitives();
    assert(s.load_stdlib(args[1]).is_ok);
    foreach (name; ["http", "uri", "ffi", "memory", "parsec", "shell", "sequence", "array",
                   "stream", "getopt", "db", "nosqldb", "transpile", "weblets", "jsonrpc"]) {
        auto result = s.eval("import \"" ~ name ~ "\"");
        if (result.is_err) {
            stderr.writeln(name);
            foreach (d; result.unwrap_err) stderr.writeln(d.message);
            return 1;
        }
    }
    foreach (source; [
        "Array.set 1 9 [1,2,3] == Some [1,9,3]",
        "Array.slice 2 2 [1,2,3] == None",
        "Stream.take 3 (Stream.unfold (fn n -> Some (n, n + 1)) 0) == [0,1,2]",
        "Getopt.parse [(\"x\", true)] [\"--x=ok\", \"--\", \"--y\"] == Ok ([(\"x\", Some \"ok\")], [\"--y\"])",
        "Db.get \"k\" (Db.put \"k\" 2 [(\"k\", 1)]) == Some 2",
        "Weblets.text \"<&\" == \"&lt;&amp;\"",
        "Shell.command (\"printf %s \" ^ Shell.quote \"a'b\") == Ok (0, \"a'b\")",
        "JsonRPC.dispatch (fn method params -> Ok params) (JsonRPC.notification \"echo\" (JArr [])) == None"
    ]) {
        auto result = s.eval(source);
        if (to_string(result) != "true") {
            stderr.writeln(source, ": ", to_string(result));
            return 1;
        }
    }
    return 0;
}
