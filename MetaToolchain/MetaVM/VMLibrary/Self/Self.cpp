#include "Self.hpp"
namespace metavm { namespace library {
const Profile& Self_profile() {
 static const Profile profile = {"Self","object","catalog-only","Prototype-based object language runtime",&Self_example_module};
 return profile;
}
} }
