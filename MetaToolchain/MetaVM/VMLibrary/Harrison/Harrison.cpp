#include "Harrison.hpp"
namespace metavm { namespace library {
const Profile& Harrison_profile() {
 static const Profile profile = {"Harrison","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&Harrison_example_module};
 return profile;
}
} }
