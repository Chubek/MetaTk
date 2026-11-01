#include "DVI.hpp"
namespace metavm { namespace library {
const Profile& DVI_profile() {
 static const Profile profile = {"DVI","document","catalog-only","TeX device independent output command interpreter",&DVI_example_module};
 return profile;
}
} }
