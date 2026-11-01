#include "WAM.hpp"
namespace metavm { namespace library {
const Profile& WAM_profile() {
 static const Profile profile = {"WAM","logic","catalog-only","Warren abstract machine for logic programs",&WAM_example_module};
 return profile;
}
} }
