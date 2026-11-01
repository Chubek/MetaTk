#include "CESK.hpp"
namespace metavm { namespace library {
const Profile& CESK_profile() {
 static const Profile profile = {"CESK","closure","catalog-only","Control, environment, store, continuation",&CESK_example_module};
 return profile;
}
} }
