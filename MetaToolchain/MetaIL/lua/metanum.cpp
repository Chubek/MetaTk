// Lua 5.4 bridge to the actual MetaNum headers. Lua errors may longjmp:
// never hold an owning C++ object across a Lua API call.
#include "lua_compat.h"
#include "all.hpp"
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
constexpr std::size_t max_samples = 1000000;

double* series(lua_State* L, int index, std::size_t& n) {
    if (lua_type(L, index) != LUA_TTABLE)
        luaL_error(L, "metanum: argument %d must be a dense numeric array", index);
    n = lua_rawlen(L, index);
    if (n > max_samples) luaL_error(L, "metanum: sample limit exceeded");
    // Reject holes and extra keys, independently of Lua's undefined sparse length.
    std::size_t count = 0;
    lua_pushnil(L);
    while (lua_next(L, index)) {
        int integer = 0;
        const lua_Integer key = lua_tointegerx(L, -2, &integer);
        if (!lua_isinteger(L, -2) || !integer || key < 1 || static_cast<unsigned long long>(key) > n ||
            lua_type(L, -1) != LUA_TNUMBER)
            luaL_error(L, "metanum: argument %d must be a dense numeric array", index);
        ++count;
        lua_settop(L, -2);
    }
    if (count != n) luaL_error(L, "metanum: sparse array at argument %d", index);
    // Lua owns temporary buffers, including on longjmp or allocation failure.
    auto* data = static_cast<double*>(lua_newuserdatauv(L, n * sizeof(double), 0));
    for (std::size_t i = 0; i < n; ++i) {
        lua_rawgeti(L, index, static_cast<lua_Integer>(i + 1));
        data[i] = lua_tonumberx(L, -1, nullptr);
        lua_settop(L, -2);
    }
    return data;
}

int compute(lua_State* L) {
    if (lua_gettop(L) != 4) return luaL_error(L, "metanum.compute expects operation, x, y, option");
    std::size_t op_len = 0;
    const char* op = luaL_checklstring(L, 1, &op_len);
    if (std::strlen(op) != op_len) return luaL_error(L, "metanum: invalid operation");
    const bool paired = !std::strcmp(op, "covariance") || !std::strcmp(op, "correlation");
    const bool sampled = !std::strcmp(op, "variance") || !std::strcmp(op, "standard_deviation") ||
                         !std::strcmp(op, "covariance");
    const bool quantile = !std::strcmp(op, "quantile");
    if (!paired && lua_type(L, 3) != LUA_TNIL) return luaL_error(L, "metanum: unexpected second series");
    if (sampled && lua_type(L, 4) != LUA_TBOOLEAN) return luaL_error(L, "metanum: sample must be boolean");
    if (quantile && lua_type(L, 4) != LUA_TNUMBER) return luaL_error(L, "metanum: probability must be numeric");
    if (!sampled && !quantile && lua_type(L, 4) != LUA_TNIL) return luaL_error(L, "metanum: unexpected option");
    const bool sample = lua_toboolean(L, 4) != 0;
    const double probability = lua_tonumberx(L, 4, nullptr);
    std::size_t nx = 0, ny = 0;
    const double* x = series(L, 2, nx);
    const double* y = paired ? series(L, 3, ny) : nullptr;
    if (paired && nx != ny) return luaL_error(L, "metanum: series differ in length");
    double value = 0;
    char error[256] = {};
    // All exceptions and scratch vectors are destroyed before reentering Lua.
    try {
        if (!std::strcmp(op, "mean")) value = metanum::mean(x, nx);
        else if (!std::strcmp(op, "variance")) value = metanum::variance(x, nx, sample);
        else if (!std::strcmp(op, "standard_deviation")) value = metanum::standard_deviation(x, nx, sample);
        else if (!std::strcmp(op, "covariance")) value = metanum::covariance(x, y, nx, sample);
        else if (!std::strcmp(op, "correlation")) value = metanum::correlation(x, y, nx);
        else if (!std::strcmp(op, "median")) value = metanum::median(x, nx);
        else if (quantile) value = metanum::quantile(x, nx, probability);
        else if (!std::strcmp(op, "min")) value = (metanum::min)(x, nx);
        else if (!std::strcmp(op, "max")) value = (metanum::max)(x, nx);
        else std::snprintf(error, sizeof error, "metanum: unknown operation %s", op);
    } catch (const std::exception& e) {
        std::snprintf(error, sizeof error, "%s", e.what());
    } catch (...) {
        std::snprintf(error, sizeof error, "metanum: unknown native exception");
    }
    if (error[0]) return luaL_error(L, "%s", error);
    lua_pushnumber(L, value);
    return 1;
}

void field(lua_State* L, const char* key, std::string_view value) {
    lua_pushlstring(L, value.data(), value.size());
    lua_setfield(L, -2, key);
}

int catalog(lua_State* L) {
    lua_createtable(L, static_cast<int>(metanum::catalog::all_categories.size()), 0);
    lua_Integer index = 0;
    for (const auto* category : metanum::catalog::all_categories) {
        lua_createtable(L, 0, 5);
        field(L, "id", category->id);
        field(L, "library", metanum::catalog::to_string(category->lib));
        field(L, "description", category->description);
        lua_pushboolean(L, 0); lua_setfield(L, -2, "callable");
        lua_createtable(L, static_cast<int>(category->size()), 0);
        lua_Integer j = 0;
        for (const auto& operation : *category) {
            lua_createtable(L, 0, 3);
            field(L, "name", operation.name);
            field(L, "symbols", operation.symbols);
            field(L, "description", operation.description);
            lua_rawseti(L, -2, ++j);
        }
        lua_setfield(L, -2, "operations");
        lua_rawseti(L, -2, ++index);
    }
    return 1;
}
} // namespace

extern "C" int luaopen_metanum_native(lua_State* L) {
    lua_createtable(L, 0, 3);
    lua_pushcclosure(L, compute, 0); lua_setfield(L, -2, "compute");
    lua_pushcclosure(L, catalog, 0); lua_setfield(L, -2, "catalog");
    lua_pushinteger(L, static_cast<lua_Integer>(max_samples)); lua_setfield(L, -2, "max_samples");
    return 1;
}
