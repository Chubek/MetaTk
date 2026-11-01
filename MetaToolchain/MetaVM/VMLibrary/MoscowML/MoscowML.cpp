#include "MoscowML.hpp"
namespace metavm { namespace library {
const Profile& MoscowML_profile() {
 static const Profile profile = {"MoscowML","closure","catalog-only","Moscow ML runtime",&MoscowML_example_module};
 return profile;
}
} }
