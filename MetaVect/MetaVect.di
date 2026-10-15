module MetaVect;
/// Public D-facing version/status contract. The full compiler API is evolving.
enum versionMajor = 0;
enum versionMinor = 1;
enum versionPatch = 0;
enum Status { ok, invalidArgument, outOfMemory, notImplemented }
string versionString() @safe pure nothrow { return "0.1.0"; }
