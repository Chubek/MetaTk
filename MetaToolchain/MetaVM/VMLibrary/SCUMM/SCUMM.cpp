#include "SCUMM.hpp"
namespace metavm { namespace library {
const Profile& SCUMM_profile() {
 static const Profile profile = {"SCUMM","script","catalog-only","Versioned game scripting machine",&SCUMM_example_module};
 return profile;
}
} }
