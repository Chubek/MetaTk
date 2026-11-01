#include "Krivine.hpp"
namespace metavm { namespace library {
const Profile& Krivine_profile() {
 static const Profile profile = {"Krivine","closure","krivine-whnf-subset","Version-1 closed de Bruijn terms; bounded call-by-name weak-head closure evaluation",&Krivine_example_module};
 return profile;
}
} }
