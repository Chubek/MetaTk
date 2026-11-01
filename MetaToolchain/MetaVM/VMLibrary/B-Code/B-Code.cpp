#include "B-Code.hpp"
namespace metavm { namespace library {
const Profile& B_Code_profile() {
 static const Profile profile = {"B-Code","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&B_Code_example_module};
 return profile;
}
} }
