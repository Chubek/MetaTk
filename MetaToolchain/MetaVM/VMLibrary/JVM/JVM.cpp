#include "JVM.hpp"
namespace metavm { namespace library {
const Profile& JVM_profile() {
 static const Profile profile = {"JVM","stack","integer-method-subset","JVM integer Code arrays with local variables and bounded operand stack",&JVM_example_module};
 return profile;
}
} }
