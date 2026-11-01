#include "SECD.hpp"
namespace metavm { namespace library {
const Profile& SECD_profile() {
 static const Profile profile = {"SECD","closure","secd-unary-subset","Version-1 unary SECD subset; lexical closures, i64 arithmetic, typed call/selection dump",&SECD_example_module};
 return profile;
}
} }
