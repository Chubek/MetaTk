#include "LuaVM.hpp"
namespace metavm { namespace library {
const Profile& LuaVM_profile() {
 static const Profile profile = {"LuaVM","register","catalog-only","Lua VM; bytecode version must be pinned",&LuaVM_example_module};
 return profile;
}
} }
