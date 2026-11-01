#include "Monty.hpp"
namespace metavm { namespace library {
const Profile& Monty_profile() {
 static const Profile profile = {"Monty","unspecified","catalog-only","No authoritative ISA or version supplied; preserve this identity pending specification",&Monty_example_module};
 return profile;
}
} }
