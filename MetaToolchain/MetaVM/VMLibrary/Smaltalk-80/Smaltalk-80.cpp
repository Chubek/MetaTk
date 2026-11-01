#include "Smaltalk-80.hpp"
namespace metavm { namespace library {
const Profile& Smaltalk_80_profile() {
 static const Profile profile = {"Smaltalk-80","object","catalog-only","Original directory spelling retained; Smalltalk-80 object machine",&Smaltalk_80_example_module};
 return profile;
}
} }
