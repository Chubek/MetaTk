#include "BCPL.hpp"
namespace metavm { namespace library {
const Profile& BCPL_profile() {
 static const Profile profile = {"BCPL","stack","catalog-only","BCPL abstract machine; dialect must be selected",&BCPL_example_module};
 return profile;
}
} }
