#include "P4.hpp"
namespace metavm { namespace library {
const Profile& P4_profile() {
 static const Profile profile = {"P4","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&P4_example_module};
 return profile;
}
} }
