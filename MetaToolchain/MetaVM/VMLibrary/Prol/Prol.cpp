#include "Prol.hpp"
namespace metavm { namespace library {
const Profile& Prol_profile() {
 static const Profile profile = {"Prol","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&Prol_example_module};
 return profile;
}
} }
