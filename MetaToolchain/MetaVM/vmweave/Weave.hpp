#ifndef METAVM_WEAVE_HPP
#define METAVM_WEAVE_HPP
#include "../vmspec/VM.hpp"

namespace metavm {
/** Compile one named MetaIL function into version-1 VM bytecode. Uses the actual
 * MetaIL parser; owns the resulting program. Other function definitions are not
 * linked: all call expressions explicitly target the Host callback namespace.
 * Rejects assumptions (no MetaIL execution contract), uninitialized reads,
 * unsupported operators, missing functions and reachable fallthrough. Throws
 * invalid_argument/length_error/runtime_error/bad_alloc. Does not mutate MetaIL
 * or install a JIT in it. Independent calls are thread safe. */
Program weave(std::string_view source, std::string_view function,
              const std::string &metail_home = {}, Limits limits = {});
}
#endif
