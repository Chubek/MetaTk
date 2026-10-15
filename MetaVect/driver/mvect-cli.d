module driver.mvect_cli;
import MetaVect : versionString;
import driver.mvect : compile, CompileOptions;
import std.stdio : stderr, writeln;

int main(string[] args) {
    CompileOptions options;
    bool positionalOnly;
    foreach (arg; args[1 .. $]) {
        if (!positionalOnly && arg == "--") { positionalOnly = true; continue; }
        if (!positionalOnly && arg == "--help") {
            writeln("mvect - MetaVect syntax frontend");
            writeln("Usage: mvect --check-syntax [--] <input.mvct>");
            writeln("       mvect --version | --help");
            writeln("Checks syntax only; types, imports, and backends are not implemented.");
            return 0;
        }
        if (!positionalOnly && arg == "--version") {
            writeln("mvect ", versionString());
            return 0;
        }
        if (!positionalOnly && arg == "--check-syntax") {
            options.checkSyntaxOnly = true;
            continue;
        }
        if (!positionalOnly && arg.length && arg[0] == '-') {
            stderr.writeln("mvect: unknown option: ", arg);
            return 2;
        }
        if (options.inputPath.length) {
            stderr.writeln("mvect: expected exactly one input file");
            return 2;
        }
        options.inputPath = arg;
    }
    if (options.inputPath.length == 0) {
        stderr.writeln("mvect: missing input file (try --help)");
        return 2;
    }
    auto result = compile(options);
    foreach (diagnostic; result.diagnostics) stderr.writeln(diagnostic);
    if (result.success) return 0;
    return options.checkSyntaxOnly ? 1 : 3;
}
