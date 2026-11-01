#include "ALEX.hpp"
namespace metavm { namespace library {
const Profile& ALEX_profile() {
 static const Profile profile = {"ALEX","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&ALEX_example_module};
 return profile;
}
} }
