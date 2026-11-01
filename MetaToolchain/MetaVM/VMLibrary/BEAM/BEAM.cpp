#include "BEAM.hpp"
namespace metavm { namespace library {
const Profile& BEAM_profile() {
 static const Profile profile = {"BEAM","register","catalog-only","Erlang instruction runtime with process scheduling",&BEAM_example_module};
 return profile;
}
} }
