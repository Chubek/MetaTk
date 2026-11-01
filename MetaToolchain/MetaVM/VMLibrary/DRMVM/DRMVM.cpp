#include "DRMVM.hpp"
namespace metavm { namespace library {
const Profile& DRMVM_profile() {
 static const Profile profile = {"DRMVM","graphics","graphics-subset","Machinery host imports into XRGB8888 surfaces; optional libdrm dumb buffers",&DRMVM_example_module};
 return profile;
}
} }
