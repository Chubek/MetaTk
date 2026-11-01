#include "ART.hpp"
namespace metavm { namespace library {
const Profile& ART_profile() {
 static const Profile profile = {"ART","runtime","catalog-only","Android runtime consuming DEX; not a separate guest instruction set",&ART_example_module};
 return profile;
}
} }
