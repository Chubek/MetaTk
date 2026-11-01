#include "CHIP-8.hpp"
namespace metavm { namespace library {
const Profile& CHIP_8_profile() {
 static const Profile profile = {"CHIP-8","register","classic-opcode-interpreter","Classic CHIP-8; legacy, COSMAC VIP and CHIP-48 opcode presets; 4 KiB memory and 64x32 display",&CHIP_8_example_module};
 return profile;
}
} }
