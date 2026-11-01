#include "Zand.hpp"
namespace metavm { namespace library {
const Profile& Zand_profile() {
 static const Profile profile = {"Zand","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&Zand_example_module};
 return profile;
}
} }
