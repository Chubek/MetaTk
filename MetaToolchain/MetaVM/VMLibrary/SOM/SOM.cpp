#include "SOM.hpp"
namespace metavm { namespace library {
const Profile& SOM_profile() {
 static const Profile profile = {"SOM","object","catalog-only","Simple object machine",&SOM_example_module};
 return profile;
}
} }
