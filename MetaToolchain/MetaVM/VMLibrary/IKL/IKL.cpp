#include "IKL.hpp"
namespace metavm { namespace library {
const Profile& IKL_profile() {
 static const Profile profile = {"IKL","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&IKL_example_module};
 return profile;
}
} }
