#include "FORTH.hpp"
namespace metavm { namespace library {
const Profile& FORTH_profile() {
 static const Profile profile = {"FORTH","stack","integer-word-subset","Data and return stacks, dictionary, colon definitions",&FORTH_example_module};
 return profile;
}
} }
