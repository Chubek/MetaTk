#include "CAM.hpp"
namespace metavm { namespace library {
const Profile& CAM_profile() {
 static const Profile profile = {"CAM","closure","catalog-only","Categorical abstract machine",&CAM_example_module};
 return profile;
}
} }
