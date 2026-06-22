Chapter 11 - Testing, Debugging, and Diagnostics
================================================

Catch2 tests cover expansion, plugins, tokenization, and syntax-file presence.

Tightening Regressions
----------------------
The test suite covers malformed identifiers, numeric guard failures, include traversal rejection, plugin guard failures, trace event shape, and SerdeTk JSON symbol serialization. CLI smoke tests should exercise ``--trace-file`` and ``--dump-symtbl`` on temporary files.
