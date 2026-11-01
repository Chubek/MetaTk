#include "Dis.hpp"
namespace metavm { namespace library {
const Profile& Dis_profile() {
 static const Profile profile = {"Dis","register","catalog-only","Inferno Dis virtual machine",&Dis_example_module};
 return profile;
}
} }
