#include "AVM2.hpp"
namespace metavm { namespace library {
const Profile& AVM2_profile() {
 static const Profile profile = {"AVM2","stack","catalog-only","ActionScript bytecode virtual machine",&AVM2_example_module};
 return profile;
}
} }
