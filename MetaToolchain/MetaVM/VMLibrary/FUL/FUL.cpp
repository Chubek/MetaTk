#include "FUL.hpp"
namespace metavm { namespace library {
const Profile& FUL_profile() {
 static const Profile profile = {"FUL","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&FUL_example_module};
 return profile;
}
} }
