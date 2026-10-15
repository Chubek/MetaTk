/// Public D-facing version/status contract. The full compiler API is evolving.
///
/// The version string and status set must match MetaVect.h and MetaVect.hpp:
/// all three variants describe the same 0.1.0 contract. See frontend/README.md for
/// why this module has no .di sibling.
module MetaVect;

enum versionMajor = 0;
enum versionMinor = 1;
enum versionPatch = 0;
enum Status { ok, invalidArgument, outOfMemory, notImplemented }

string versionString() @safe pure nothrow {
    return "0.1.0";
}