#include "FAM.hpp"
namespace metavm { namespace library {
const Profile& FAM_profile() {
 static const Profile profile = {"FAM","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&FAM_example_module};
 return profile;
}
} }
