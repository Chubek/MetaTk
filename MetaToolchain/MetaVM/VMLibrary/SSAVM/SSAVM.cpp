#include "SSAVM.hpp"
namespace metavm { namespace library {
const Profile& SSAVM_profile() {
 static const Profile profile = {"SSAVM","ssa","issa-subset","Bounded i64 ISSA subset; opt-in definite-assignment/type analysis; CEN selection; simultaneous PHI/PFE; persistent arrays",&SSAVM_example_module};
 return profile;
}
} }
