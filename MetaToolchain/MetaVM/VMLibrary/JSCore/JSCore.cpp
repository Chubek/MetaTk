#include "JSCore.hpp"
namespace metavm { namespace library {
const Profile& JSCore_profile() {
 static const Profile profile = {"JSCore","runtime","catalog-only","JavaScriptCore runtime; internal bytecode must be versioned",&JSCore_example_module};
 return profile;
}
} }
