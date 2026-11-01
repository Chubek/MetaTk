#include "Lucide.hpp"
namespace metavm { namespace library {
const Profile& Lucide_profile() {
 static const Profile profile = {"Lucide","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&Lucide_example_module};
 return profile;
}
} }
