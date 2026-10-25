module driver.mvect;
/// Driver orchestration contract. Keep argument parsing separate from compilation.
import frontend.Input : SourceText;
import frontend.Parser : parse;
import frontend.Semacheck_Visitor : checkSemantically;
import frontend.SyntaxError : formatDiagnostic;
import std.file : read, FileException;

struct CompileOptions {
    string inputPath;
    string backend = "auto";
    bool optimize = true;
    bool debugIR = false;
    /// Stop after parsing. Success does not imply semantic validity.
    bool checkSyntaxOnly = false;
}
struct CompileResult {
    bool success;
    string outputPath;
    string[] diagnostics;
}
CompileResult compile(CompileOptions options) {
    if (options.inputPath.length == 0)
        return CompileResult(false, "", ["input path must not be empty"]);
    string text;
    // The newly allocated buffer has no other owners. Leave encoding validation
    // to the lexer, which can report a source span instead of throwing UTFException.
    try { text = cast(string) read(options.inputPath); }
    catch (FileException error) {
        return CompileResult(false, "", ["cannot read input: " ~ error.msg]);
    }
    auto result = parse(new SourceText(text, options.inputPath));
    if (!result.success) {
        string[] diagnostics;
        foreach (diagnostic; result.diagnostics) diagnostics ~= formatDiagnostic(diagnostic);
        return CompileResult(false, "", diagnostics);
    }
    // Second tranche de-stub: name resolution and duplicate checks are now real.
    // Type checking and downstream stages remain unavailable by design.
    auto semantic = checkSemantically(result.ast);
    if (!semantic.success) {
        string[] diagnostics;
        foreach (diagnostic; semantic.errors) diagnostics ~= formatDiagnostic(diagnostic);
        return CompileResult(false, "", diagnostics);
    }
    if (options.checkSyntaxOnly) return CompileResult(true, "", []);
    return CompileResult(false, "", [
        "semantic analysis, MVIR lowering, and backend emission are not yet implemented; " ~
        "use --check-syntax to validate syntax only"
    ]);
}
