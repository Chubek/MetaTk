#include "Z-Machine.hpp"
namespace metavm { namespace library {
const Profile& Z_Machine_profile() {
 static const Profile profile = {"Z-Machine","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&Z_Machine_example_module};
 return profile;
}
} }
