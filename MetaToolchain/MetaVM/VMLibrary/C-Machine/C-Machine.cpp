#include "C-Machine.hpp"
namespace metavm { namespace library {
const Profile& C_Machine_profile() {
 static const Profile profile = {"C-Machine","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&C_Machine_example_module};
 return profile;
}
} }
