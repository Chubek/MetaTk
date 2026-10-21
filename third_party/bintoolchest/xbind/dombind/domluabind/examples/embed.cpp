#include <domluabind/domluabind.hpp>
int main() {
    domluabind::state lua;
    lua.eval("return 6 * 7", 1);
    int ok = 0;
    auto result = luaconv_integer(lua.get(), -1, &ok);
    return ok && result == 42 ? 0 : 1;
}
