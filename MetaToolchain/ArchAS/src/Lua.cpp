#include "Internal.hpp"
#include <cstdio>
#include <cstdlib>
#if ARCHAS_LUA
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#if LUA_VERSION_NUM < 503
#error ArchAS requires Lua 5.3 or newer
#endif
#endif

namespace archas {
bool has_lua() noexcept { return ARCHAS_LUA != 0; }
namespace detail {
#if ARCHAS_LUA
namespace {
std::string lua_quote(std::string_view s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c >= 32 && c < 127) out += char(c);
        else { out += '\\'; out += char('0' + c / 100); out += char('0' + c / 10 % 10); out += char('0' + c % 10); }
    }
    return out + '"';
}
size_t tag_end(std::string_view text, size_t pos, std::string_view end) {
    size_t braces = 0;
    while (pos < text.size()) {
        char c = text[pos];
        if (!braces && text.substr(pos).starts_with(end)) return pos;
        if (c == '\'' || c == '"') {
            char quote = c; ++pos;
            while (pos < text.size()) {
                if (text[pos] == '\\') { pos += 2; continue; }
                if (text[pos++] == quote) break;
            }
            continue;
        }
        bool comment = text.substr(pos).starts_with("--");
        size_t open = comment ? pos + 2 : pos;
        if (open < text.size() && text[open] == '[') {
            size_t j = open + 1;
            while (j < text.size() && text[j] == '=') ++j;
            if (j < text.size() && text[j] == '[') {
                auto closing = "]" + std::string(j - open - 1, '=') + "]";
                auto found = text.find(closing, j + 1);
                if (found == text.npos) return text.npos;
                pos = found + closing.size(); continue;
            }
        }
        if (comment) { pos = text.find('\n', pos); if (pos == text.npos) return pos; continue; }
        if (end == "}}") {
            if (c == '{') ++braces;
            else if (c == '}' && braces) --braces;
        }
        ++pos;
    }
    return text.npos;
}
} // namespace
struct Lua::Impl {
    const mair::Target &target;
    const Options &options;
    Directives &directives;
    lua_State *state = nullptr;
    size_t memory = 0, remaining;
    std::string output;
    std::vector<Location> output_map, code_map;
    Location current;
    bool new_line = true, in_directive = false;

    static void *allocate(void *user, void *ptr, size_t old, size_t size) {
        auto &self = *static_cast<Impl *>(user);
        if (!ptr) old = 0;
        if (!size) { self.memory -= old; std::free(ptr); return nullptr; }
        size_t used = self.memory - old;
        if (used > self.options.limits.lua_memory || size > self.options.limits.lua_memory - used) return nullptr;
        void *result = std::realloc(ptr, size);
        if (result) self.memory = used + size;
        return result;
    }
    static Impl &self(lua_State *L) { return **static_cast<Impl **>(lua_getextraspace(L)); }
    static void hook(lua_State *L, lua_Debug *) {
        auto &s = self(L);
        if (s.remaining < 1000) luaL_error(L, "Lua instruction limit exceeded");
        s.remaining -= 1000;
    }
    Location where(lua_State *L) const {
        if (in_directive) return current;
        lua_Debug info{};
        if (lua_getstack(L, 1, &info) && lua_getinfo(L, "l", &info) && info.currentline > 0 &&
            size_t(info.currentline) <= code_map.size()) return code_map[size_t(info.currentline) - 1];
        return current;
    }
    void put(std::string_view text, Location loc, bool advance) {
        append(output, text, options.limits.bytes);
        for (char c : text) {
            if (new_line) {
                if (output_map.size() >= options.limits.statements) throw Error("Lua output line limit exceeded", loc);
                output_map.push_back(loc); new_line = false;
            }
            if (c == '\n') { new_line = true; if (advance) ++loc.line; loc.column = 1; }
        }
    }
    static int literal(lua_State *L) {
        size_t length = 0;
        const char *text = luaL_checklstring(L, 1, &length);
        auto line = luaL_checkinteger(L, 2);
        bool advance = lua_toboolean(L, 3) != 0;
        char error[512];
        try {
            auto &s = self(L);
            s.put({text, length}, {s.current.file, size_t(line), 1}, advance);
            return 0;
        } catch (const std::exception &e) { std::snprintf(error, sizeof error, "%s", e.what()); }
        lua_pushstring(L, error);
        return lua_error(L);
    }
    static int emit(lua_State *L) {
        size_t length = 0;
        const char *text = luaL_checklstring(L, 1, &length);
        char error[512];
        try {
            auto &s = self(L);
            auto loc = s.where(L);
            s.put({text, length}, loc, false);
            if (!length || text[length - 1] != '\n') s.put("\n", loc, false);
            return 0;
        } catch (const std::exception &e) { std::snprintf(error, sizeof error, "%s", e.what()); }
        lua_pushstring(L, error);
        return lua_error(L);
    }
    static int register_directive(lua_State *L) {
        const char *name = luaL_checkstring(L, 1);
        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_pushvalue(L, 2);
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        char error[512];
        try {
            auto &s = self(L);
            auto key = directive_name(name);
            if (s.directives.contains(key)) throw Error("duplicate directive: " + key);
            s.directives.emplace(key, [&s, ref](std::string_view args, const DirectiveContext &ctx) {
                return s.invoke(ref, args, ctx);
            });
            return 0;
        } catch (const std::exception &e) {
            luaL_unref(L, LUA_REGISTRYINDEX, ref);
            std::snprintf(error, sizeof error, "%s", e.what());
        }
        // Leave the C++ catch scope before a Lua allocation can longjmp.
        lua_pushstring(L, error);
        return lua_error(L);
    }
    static int register_info(lua_State *L) {
        const char *name = luaL_checkstring(L, 1);
        const auto *reg = self(L).target.spec().find_register(name);
        if (!reg) { lua_pushnil(L); return 1; }
        lua_createtable(L, 0, 4);
        lua_pushstring(L, reg->name); lua_setfield(L, -2, "name");
        lua_pushinteger(L, reg->enc); lua_setfield(L, -2, "enc");
        lua_pushinteger(L, reg->width); lua_setfield(L, -2, "width");
        lua_pushstring(L, dsym_reg_class_name(reg->reg_class)); lua_setfield(L, -2, "class");
        return 1;
    }
    static int initialize(lua_State *L) {
        auto &s = self(L);
        luaL_openlibs(L);
        lua_newtable(L);
        lua_pushcfunction(L, emit); lua_setfield(L, -2, "emit");
        lua_pushcfunction(L, register_directive); lua_setfield(L, -2, "directive");
        lua_pushcfunction(L, register_info); lua_setfield(L, -2, "register");
        lua_pushstring(L, s.target.name().c_str()); lua_setfield(L, -2, "target");
        lua_pushstring(L, s.options.syntax == Syntax::intel ? "intel" : "att"); lua_setfield(L, -2, "syntax");
        lua_pushinteger(L, lua_Integer(s.target.index())); lua_setfield(L, -2, "target_index");
        lua_newtable(L);
        for (const auto &[name, value] : s.options.defines) {
            lua_pushinteger(L, value); lua_setfield(L, -2, name.c_str());
        }
        lua_setfield(L, -2, "defines");
        lua_setglobal(L, "domas");
        lua_pushcfunction(L, literal); lua_setglobal(L, "__archas_literal");
        return 0;
    }
    Impl(const mair::Target &t, const Options &o, Directives &d)
        : target(t), options(o), directives(d), remaining(o.limits.lua_instructions) {
#if LUA_VERSION_NUM >= 505
        state = lua_newstate(allocate, this, 0x41534153u);
#else
        state = lua_newstate(allocate, this);
#endif
        if (!state) throw Error("cannot create Lua state within memory limit");
        *static_cast<Impl **>(lua_getextraspace(state)) = this;
        lua_sethook(state, hook, LUA_MASKCOUNT, 1000);
        lua_pushcfunction(state, initialize);
        if (lua_pcall(state, 0, 0, 0) != LUA_OK) {
            std::string message = lua_tostring(state, -1) ? lua_tostring(state, -1) : "Lua initialization failed";
            lua_close(state); state = nullptr; throw Error(message);
        }
    }
    ~Impl() { if (state) lua_close(state); }
    void reset(Location loc) {
        output.clear(); output_map.clear(); code_map.clear(); new_line = true; current = std::move(loc);
    }
    [[noreturn]] void failure() {
        const char *text = lua_tostring(state, -1);
        std::string message = text ? text : "Lua error (non-string error object)";
        Location loc = current;
        auto prefix = current.file + ":";
        auto begin = message.find(prefix);
        if (!in_directive && begin != message.npos) {
            begin += prefix.size();
            size_t line = 0;
            auto r = std::from_chars(message.data() + begin, message.data() + message.size(), line);
            if (r.ec == std::errc{} && line > 0 && line <= code_map.size()) loc = code_map[line - 1];
        }
        lua_pop(state, 1);
        throw Error("Lua: " + message, loc);
    }
    void execute(std::string_view code, const std::string &file) {
        std::string name = "@" + file;
        if (luaL_loadbufferx(state, code.data(), code.size(), name.c_str(), "t") != LUA_OK) failure();
        if (lua_pcall(state, 0, 0, 0) != LUA_OK) failure();
    }
    std::string invoke(int ref, std::string_view args, const DirectiveContext &ctx) {
        reset(ctx.location); in_directive = true;
        // Argument construction is inside a protected Lua call too: a small
        // configured allocator budget must never trigger an unprotected panic.
        struct Call { int ref; std::string_view args; const DirectiveContext *ctx; } call{ref, args, &ctx};
        lua_pushcfunction(state, [](lua_State *L) -> int {
            const auto &c = *static_cast<Call *>(lua_touserdata(L, 1));
            lua_rawgeti(L, LUA_REGISTRYINDEX, c.ref);
            lua_pushlstring(L, c.args.data(), c.args.size());
            lua_createtable(L, 0, 5);
            lua_pushstring(L, c.ctx->location.file.c_str()); lua_setfield(L, -2, "file");
            lua_pushinteger(L, lua_Integer(c.ctx->location.line)); lua_setfield(L, -2, "line");
            lua_pushinteger(L, lua_Integer(c.ctx->location.column)); lua_setfield(L, -2, "column");
            lua_pushstring(L, c.ctx->target.name().c_str()); lua_setfield(L, -2, "target");
            lua_pushstring(L, c.ctx->syntax == Syntax::intel ? "intel" : "att"); lua_setfield(L, -2, "syntax");
            lua_call(L, 2, 1); return 1;
        });
        lua_pushlightuserdata(state, &call);
        if (lua_pcall(state, 1, 1, 0) != LUA_OK) failure();
        if (!lua_isnil(state, -1)) {
            if (lua_type(state, -1) != LUA_TSTRING) { lua_pop(state, 1); throw Error("Lua directive must return a string or nil", ctx.location); }
            size_t length = 0;
            const char *text = lua_tolstring(state, -1, &length);
            put({text, length}, ctx.location, false);
        }
        lua_pop(state, 1); in_directive = false;
        return std::move(output);
    }
};
Lua::Lua(const mair::Target &t, const Options &o, Directives &d) : impl_(std::make_unique<Impl>(t, o, d)) {}
Lua::~Lua() = default;
std::string Lua::script(std::string_view source, const std::string &file) {
    auto &s = *impl_; s.reset({file, 1, 1});
    s.code_map = std::vector<Location>(1, s.current);
    for (char c : source) if (c == '\n') s.code_map.push_back({file, s.code_map.size() + 1, 1});
    s.execute(source, file);
    return std::move(s.output);
}
Lines Lua::expand(std::string_view source, const Location &location) {
    if (source.find("{{") == source.npos && source.find("{%") == source.npos && source.find("{#") == source.npos)
        return raw_lines(source, location);
    auto &s = *impl_; s.reset(location);
    std::string code;
    size_t position = 0, line = location.line;
    size_t code_limit = s.options.limits.bytes > SIZE_MAX / 8 ? SIZE_MAX : s.options.limits.bytes * 8;
    auto code_add = [&](std::string text, size_t origin, bool advance) {
        if (text.empty() || text.back() != '\n') text += '\n';
        append(code, text, code_limit);
        for (char c : text) if (c == '\n') {
            s.code_map.push_back({location.file, origin, 1});
            if (advance) ++origin;
        }
    };
    while (position < source.size()) {
        size_t begin = source.find('{', position);
        while (begin != source.npos && !source.substr(begin).starts_with("{{") &&
               !source.substr(begin).starts_with("{%") && !source.substr(begin).starts_with("{#")) begin = source.find('{', begin + 1);
        if (begin == source.npos) begin = source.size();
        auto literal = source.substr(position, begin - position);
        if (!literal.empty()) code_add("__archas_literal(" + lua_quote(literal) + "," + std::to_string(line) + ",true);", line, false);
        line += size_t(std::count(literal.begin(), literal.end(), '\n'));
        if (begin == source.size()) break;
        bool expression = source[begin + 1] == '{', comment = source[begin + 1] == '#';
        auto end = comment ? source.find("#}", begin + 2) : tag_end(source, begin + 2, expression ? "}}" : "%}");
        if (end == source.npos) throw Error("unterminated Lua template tag", {location.file, line, 1});
        auto text = source.substr(begin + 2, end - begin - 2);
        if (expression) code_add("__archas_literal(tostring((" + std::string(text) + "))," + std::to_string(line) + ",false);", line, true);
        else if (!comment) code_add(std::string(text), line, true);
        line += size_t(std::count(text.begin(), text.end(), '\n'));
        position = end + 2;
    }
    s.execute(code, location.file);
    auto lines = raw_lines(s.output, location);
    for (size_t i = 0; i < lines.size() && i < s.output_map.size(); ++i) lines[i].location = s.output_map[i];
    return lines;
}
#else
struct Lua::Impl {};
Lua::Lua(const mair::Target &, const Options &, Directives &) {}
Lua::~Lua() = default;
std::string Lua::script(std::string_view, const std::string &file) { throw Error("Lua support was disabled at build time", {file, 1, 1}); }
Lines Lua::expand(std::string_view source, const Location &location) {
    if (source.find("{{") != source.npos || source.find("{%") != source.npos || source.find("{#") != source.npos)
        throw Error("Lua support was disabled at build time", location);
    return raw_lines(source, location);
}
#endif
} // namespace detail
} // namespace archas
