#include "DVI.hpp"
namespace metavm { namespace library {
const Profile& DVI_profile() {
 static const Profile profile = {"DVI","document","dvi2-interpreter","Standard DVI format 2 decoder and integer-coordinate interpreter; external font metrics and opaque specials",&DVI_example_module};
 return profile;
}
} }
