#include "TAMMY.hpp"
namespace metavm { namespace library {
const Profile& TAMMY_profile() {
 static const Profile profile = {"TAMMY","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&TAMMY_example_module};
 return profile;
}
} }
