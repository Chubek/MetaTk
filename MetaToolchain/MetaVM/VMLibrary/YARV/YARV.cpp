#include "YARV.hpp"
namespace metavm { namespace library {
const Profile& YARV_profile() {
 static const Profile profile = {"YARV","stack","catalog-only","Ruby instruction sequences; version must be pinned",&YARV_example_module};
 return profile;
}
} }
