#include "G-Code.hpp"
namespace metavm { namespace library {
const Profile& G_Code_profile() {
 static const Profile profile = {"G-Code","motion","catalog-only","Modal machine control command interpreter; dialect must be selected",&G_Code_example_module};
 return profile;
}
} }
