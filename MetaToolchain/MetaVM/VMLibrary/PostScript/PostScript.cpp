#include "PostScript.hpp"
namespace metavm { namespace library {
const Profile& PostScript_profile() {
 static const Profile profile = {"PostScript","stack","catalog-only","PostScript operand, dictionary and execution stacks",&PostScript_example_module};
 return profile;
}
} }
