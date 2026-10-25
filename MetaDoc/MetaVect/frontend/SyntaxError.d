/// Structured frontend errors; presentation is kept separate from detection.
module frontend.SyntaxError;

public import frontend.Input : SourceLocation, SourceSpan;
import std.format : format;

enum DiagnosticKind {
    invalidEncoding,
    invalidCharacter,
    invalidNumber,
    invalidString,
    unterminatedComment,
    unexpectedToken,
    nestingLimit,
    undefinedName,
    duplicateDefinition,
    unsupportedNode
}

struct Diagnostic {
    string message;
    SourceSpan span;
    DiagnosticKind kind;

    @property SourceLocation location() const { return span.start; }
}

string formatDiagnostic(Diagnostic diagnostic) {
    const at = diagnostic.location;
    return format("%s:%s:%s: error: %s", at.file, at.line, at.column,
                  diagnostic.message);
}

string moduleName() @safe pure nothrow { return "frontend.SyntaxError"; }
