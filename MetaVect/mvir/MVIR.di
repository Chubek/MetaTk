/// Public MVIR node kinds and rewrite contract.
module mvir.MVIR.;
/// This module is an implementation boundary; concrete compiler integration is intentionally explicit.
struct SourceLocation { string file; uint line; uint column; }
struct Diagnostic { string message; SourceLocation location; }
/// Return a stable module identifier useful in diagnostics and debug dumps.
string moduleName() @safe pure nothrow { return "mvir.MVIR."; }
