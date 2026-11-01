#include "P-Code.hpp"
namespace metavm { namespace library {
const Profile& P_Code_profile() {
 static const Profile profile = {"P-Code","stack","catalog-only","Pascal p-code; dialect must be selected",&P_Code_example_module};
 return profile;
}
} }
