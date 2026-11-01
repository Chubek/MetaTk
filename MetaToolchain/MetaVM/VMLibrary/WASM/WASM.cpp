#include "WASM.hpp"
namespace metavm { namespace library {
const Profile& WASM_profile() {
 static const Profile profile = {"WASM","stack","catalog-only","WebAssembly typed instructions and linear memory",&WASM_example_module};
 return profile;
}
} }
