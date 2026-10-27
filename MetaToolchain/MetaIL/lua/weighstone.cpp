// No owning C++ object may survive a Lua API call (Lua errors can longjmp).
#include "lua_compat.h"
#include "weighstone_facts.hpp"
#include <Weighstone/CCINT/NumberTheory.hpp>
#include <cstdio>
#include <cstring>

namespace {
using metail::weighstone_detail::Facts;
Facts read_facts(lua_State* L, int index) {
    Facts f;
    if (lua_type(L, index) == LUA_TNIL) return f;
    if (lua_type(L, index) != LUA_TTABLE) luaL_error(L, "weighstone: facts must be a table");
    std::int64_t* fields[] = {&f.lo, &f.hi, &f.zero, &f.one};
    const char* names[] = {"lo", "hi", "zero", "one"};
    for (int i = 0; i < 4; ++i) {
        lua_getfield(L, index, names[i]);
        if (!lua_isinteger(L, -1)) luaL_error(L, "weighstone: integer fact %s required", names[i]);
        *fields[i] = luaL_checkinteger(L, -1);
        lua_settop(L, -2);
    }
    return f;
}
int transfer(lua_State* L) {
    if (lua_gettop(L) != 3) return luaL_error(L, "weighstone.transfer expects op, lhs facts, rhs facts");
    std::size_t len = 0;
    const char* op = luaL_checklstring(L, 1, &len);
    if (std::strlen(op) != len) return luaL_error(L, "weighstone: invalid operator");
    const Facts a = read_facts(L, 2), b = read_facts(L, 3);
    Facts result;
    char error[256] = {};
    try {
        result = metail::weighstone_detail::transfer(op, a, b);
    } catch (const std::exception& e) {
        std::snprintf(error, sizeof error, "weighstone: %s", e.what());
    } catch (...) {
        std::snprintf(error, sizeof error, "weighstone: unknown native exception");
    }
    if (error[0]) return luaL_error(L, "%s", error);
    lua_createtable(L, 0, 5);
    lua_pushinteger(L, result.lo); lua_setfield(L, -2, "lo");
    lua_pushinteger(L, result.hi); lua_setfield(L, -2, "hi");
    lua_pushinteger(L, result.zero); lua_setfield(L, -2, "zero");
    lua_pushinteger(L, result.one); lua_setfield(L, -2, "one");
    lua_pushboolean(L, result.total); lua_setfield(L, -2, "total");
    return 1;
}
int inverse(lua_State* L) {
    if (lua_gettop(L) != 1 || !lua_isinteger(L, 1))
        return luaL_error(L, "weighstone.inverse expects one signed-64 integer");
    const auto value = luaL_checkinteger(L, 1);
    bool exists = false;
    std::int64_t result = 0;
    char error[256] = {};
    try {
        using namespace weighstone::ccint;
        const auto v = mod_inverse(BigInt(value), power_of_two(64));
        exists = v.has_value();
        if (v) result = metail::weighstone_detail::signed_bits(*v);
    } catch (const std::exception& e) {
        std::snprintf(error, sizeof error, "weighstone: %s", e.what());
    } catch (...) {
        std::snprintf(error, sizeof error, "weighstone: unknown native exception");
    }
    if (error[0]) return luaL_error(L, "%s", error);
    if (exists) lua_pushinteger(L, result); else lua_pushnil(L);
    return 1;
}
}
extern "C" int luaopen_weighstone_native(lua_State* L) {
    lua_createtable(L, 0, 2);
    lua_pushcclosure(L, transfer, 0); lua_setfield(L, -2, "transfer");
    lua_pushcclosure(L, inverse, 0); lua_setfield(L, -2, "inverse");
    return 1;
}
