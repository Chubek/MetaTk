#include "ParrotVM.hpp"
namespace metavm { namespace library {
const Profile& ParrotVM_profile() {
 static const Profile profile = {"ParrotVM","register","catalog-only","Parrot virtual machine",&ParrotVM_example_module};
 return profile;
}
} }
