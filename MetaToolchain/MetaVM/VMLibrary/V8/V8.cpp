#include "V8.hpp"
namespace metavm { namespace library {
const Profile& V8_profile() {
 static const Profile profile = {"V8","runtime","catalog-only","JavaScript engine; no stable external bytecode ABI",&V8_example_module};
 return profile;
}
} }
