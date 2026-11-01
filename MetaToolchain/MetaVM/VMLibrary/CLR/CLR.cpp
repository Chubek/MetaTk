#include "CLR.hpp"
namespace metavm { namespace library {
const Profile& CLR_profile() {
 static const Profile profile = {"CLR","stack","catalog-only","Common intermediate language runtime",&CLR_example_module};
 return profile;
}
} }
