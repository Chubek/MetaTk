#include "Dalvik.hpp"
namespace metavm { namespace library {
const Profile& Dalvik_profile() {
 static const Profile profile = {"Dalvik","register","catalog-only","DEX register instructions",&Dalvik_example_module};
 return profile;
}
} }
