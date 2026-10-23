#include "DSLtk.hpp"
#include <cassert>

int main() {
  auto state = luaL_newstate();
  assert(state);
  dsl::Preprocessor pp;
  pp.script = dsl::lua_script_hook(state);
  assert(pp.process("#lua return '(= a b)'\n") == "(= a b)\n");
  assert(lua_gettop(state) == 0);
  bool failed = false;
  try { pp.process("#lua !invalid!\n"); } catch (const std::exception&) { failed = true; }
  assert(failed && lua_gettop(state) == 0);
  lua_close(state);
}
