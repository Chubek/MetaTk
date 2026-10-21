#include "xbind_codegen.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

namespace xbind::codegen {
namespace {
using namespace qamrpp;

std::string quote(const std::string& value) {
  std::ostringstream out;
  out << '"';
  for (unsigned char c : value) {
    if (c == '"' || c == '\\') out << '\\' << char(c);
    else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c);
    else out << char(c);
  }
  return out.str() + '"';
}

void options(std::ostream& out, const Options& values) {
  out << '{';
  bool first = true;
  for (const auto& [key, value] : values) {
    if (!first) out << ',';
    first = false;
    out << quote(key) << ':';
    if (key == "nullable") out << value;
    else out << quote(value);
  }
  out << '}';
}

void strings(std::ostream& out, const std::vector<std::string>& values) {
  out << '[';
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << quote(values[i]);
  }
  out << ']';
}

void typedefs(std::ostream& out, const std::vector<Typedef>& values) {
  out << '[';
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << "{\"c\":" << quote(values[i].c_type) << ",\"native\":" << quote(values[i].native_type)
        << ",\"options\":";
    options(out, values[i].options);
    out << '}';
  }
  out << ']';
}

// Small, strict S-expression reader for the committed default type maps.
// Unlike substring matching, it diagnoses malformed forms and quoted escapes.
struct Sexp {
  std::string atom;
  std::vector<Sexp> children;
  bool list = false;
};
class SexpReader {
  std::string text;
  size_t pos = 0;
  void space() {
    while (pos < text.size()) {
      if (std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
      else if (text[pos] == ';') {
        while (pos < text.size() && text[pos] != '\n') ++pos;
      } else break;
    }
  }
  Sexp parse(size_t depth) {
    space();
    if (pos == text.size() || depth > 64) throw std::runtime_error("invalid typedef S-expression");
    Sexp result;
    char c = text[pos++];
    if (c == '(') {
      result.list = true;
      for (;;) {
        space();
        if (pos == text.size()) throw std::runtime_error("unterminated typedef form");
        if (text[pos] == ')') { ++pos; break; }
        result.children.push_back(parse(depth + 1));
      }
    } else if (c == '"') {
      bool closed = false;
      while (pos < text.size()) {
        c = text[pos++];
        if (c == '"') { closed = true; break; }
        if (c == '\\') {
          if (pos == text.size()) break;
          c = text[pos++];
          if (c != '"' && c != '\\') throw std::runtime_error("unsupported typedef escape");
        }
        result.atom += c;
      }
      if (!closed) throw std::runtime_error("unterminated typedef string");
    } else {
      if (c == ')') throw std::runtime_error("unexpected typedef close parenthesis");
      result.atom += c;
      while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos])) &&
             text[pos] != '(' && text[pos] != ')') result.atom += text[pos++];
    }
    return result;
  }
public:
  explicit SexpReader(std::string input) : text(std::move(input)) {}
  Sexp read() {
    auto result = parse(0);
    space();
    if (pos != text.size()) throw std::runtime_error("trailing typedef input");
    return result;
  }
};

std::map<std::string, Typedef> lua_types(const Manifest& manifest, const std::string& root) {
  std::ifstream file(root + "/typedefs/Lua-Default.sexpr");
  if (!file) throw std::runtime_error("cannot read Lua default typedef map at " + root);
  std::ostringstream contents;
  contents << file.rdbuf();
  auto tree = SexpReader(contents.str()).read();
  if (!tree.list || tree.children.empty() || tree.children[0].atom != "xbind-typedefs")
    throw std::runtime_error("expected xbind-typedefs form");
  std::map<std::string, Typedef> result;
  bool target = false;
  for (size_t i = 1; i < tree.children.size(); ++i) {
    const auto& node = tree.children[i];
    if (!node.list || node.children.empty()) throw std::runtime_error("invalid typedef entry");
    const auto& tag = node.children[0].atom;
    if (tag == "target") {
      if (node.children.size() != 2 || node.children[1].atom != "Lua" || target)
        throw std::runtime_error("invalid Lua typedef target");
      target = true;
    } else if (tag == "map") {
      Options fields;
      for (size_t j = 1; j < node.children.size(); ++j) {
        const auto& field = node.children[j];
        if (!field.list || field.children.size() != 2 || field.children[1].list ||
            !fields.emplace(field.children[0].atom, field.children[1].atom).second)
          throw std::runtime_error("invalid typedef map field");
      }
      if (!fields.count("c") || !fields.count("native")) throw std::runtime_error("incomplete typedef map");
      Typedef type{fields.at("c"), fields.at("native"), fields};
      if (!result.emplace(type.c_type, type).second) throw std::runtime_error("duplicate default typedef");
    } else if (tag != "version" && tag != "description") {
      throw std::runtime_error("unknown typedef form: " + tag);
    }
  }
  if (!target) throw std::runtime_error("missing Lua typedef target");
  for (const auto& type : manifest.typedefs) result[type.c_type] = type;
  auto lua = std::find_if(manifest.target_options.begin(), manifest.target_options.end(),
                         [](const Target& t) { return t.name == "Lua"; });
  if (lua == manifest.target_options.end()) throw std::runtime_error("spec does not select Lua");
  for (const auto& type : lua->typedefs) result[type.c_type] = type;
  return result;
}

std::string option(const Options& opts, const std::string& key) {
  auto it = opts.find(key);
  return it == opts.end() ? "" : it->second;
}
} // namespace

std::string dump_ir(const qamrpp::Manifest& m) {
  std::ostringstream out;
  out << "{\"ir_version\":1,\"spec\":" << quote(m.path) << ",\"library\":" << quote(m.name)
      << ",\"version\":" << quote(m.version) << ",\"headers\":";
  strings(out, m.headers);
  out << ",\"stdlib\":"; strings(out, m.stdlib_modules);
  out << ",\"features\":"; strings(out, m.features);
  out << ",\"typedefs\":"; typedefs(out, m.typedefs);
  out << ",\"targets\":[";
  for (size_t i = 0; i < m.target_options.size(); ++i) {
    if (i) out << ',';
    const auto& t = m.target_options[i];
    out << "{\"name\":" << quote(t.name) << ",\"options\":"; options(out, t.options);
    out << ",\"typedefs\":"; typedefs(out, t.typedefs);
    out << ",\"features\":"; strings(out, t.features);
    out << '}';
  }
  out << "],\"functions\":[";
  for (size_t i = 0; i < m.functions.size(); ++i) {
    if (i) out << ',';
    const auto& f = m.functions[i];
    out << "{\"name\":" << quote(f.name) << ",\"returns\":" << quote(f.returns) << ",\"options\":";
    options(out, f.options);
    out << ",\"features\":"; strings(out, f.features);
    out << ",\"params\":[";
    for (size_t j = 0; j < f.params.size(); ++j) {
      if (j) out << ',';
      const auto& p = f.params[j];
      out << "{\"name\":" << quote(p.name) << ",\"c\":" << quote(p.c_type) << ",\"options\":";
      options(out, p.options);
      out << '}';
    }
    out << "]}";
  }
  return out.str() + "]}\n";
}

std::string emit_check(const qamrpp::Manifest& m) {
  std::ostringstream out;
  out << "// Generated by XBind: compile as C++20; no link step required.\n#include <type_traits>\n";
  for (const auto& header : m.headers) out << "#include " << quote(header) << '\n';
  for (const auto& f : m.functions) {
    out << "static_assert(std::is_same_v<decltype(&" << f.name << "), " << f.returns << " (*)(";
    for (size_t i = 0; i < f.params.size(); ++i) {
      if (i) out << ", ";
      out << f.params[i].c_type;
    }
    out << ")>, \"XBind signature mismatch: " << f.name << "\");\n";
  }
  return out.str();
}

std::string emit_lua(const qamrpp::Manifest& m, const std::string& root) {
  if (m.functions.empty()) throw std::runtime_error("inventory-only spec has no C functions to emit");
  auto types = lua_types(m, root);
  auto target = std::find_if(m.target_options.begin(), m.target_options.end(),
                            [](const qamrpp::Target& t) { return t.name == "Lua"; });
  auto module = option(target->options, "module");
  if (module.empty()) module = m.name;
  if (module.empty() || (!std::isalpha(static_cast<unsigned char>(module[0])) && module[0] != '_') ||
      module.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != std::string::npos)
    throw std::runtime_error("Lua module must be a C identifier: " + module);
  auto lower = [&](const std::string& c, const Options& overrides, bool returning) {
    auto it = types.find(c);
    if (it == types.end()) throw std::runtime_error("unmapped Lua type: " + c);
    auto opts = it->second.options;
    for (const auto& [k, v] : overrides) opts[k] = v;
    auto native = it->second.native_type;
    if (native == "int") native = "integer";
    if (native == "opaque") native = "lightuserdata";
    if (option(opts, "ownership") == "out_string" ||
        (!option(opts, "direction").empty() && option(opts, "direction") != "return" && option(opts, "direction") != "in"))
      throw std::runtime_error("Lua out-buffer lowering is not supported: " + c);
    if (native == "string" && (c != "const char *" || option(opts, "ownership") != "borrowed"))
      throw std::runtime_error("Lua strings require borrowed const char *; use opaque for buffers: " + c);
    if (native != "nil" && native != "integer" && native != "number" && native != "boolean" &&
        native != "string" && native != "lightuserdata") throw std::runtime_error("unsupported Lua native type: " + native);
    if (native == "nil" && c != "void") throw std::runtime_error("nil mapping requires void: " + c);
    if (native == "lightuserdata" && c.find('*') == std::string::npos)
      throw std::runtime_error("opaque mapping requires a pointer: " + c);
    if (!returning && native == "nil") throw std::runtime_error("void Lua parameter");
    return std::pair{native, option(opts, "nullable") == "true"};
  };
  std::ostringstream out;
  out << emit_check(m);
  out << R"CPP(#include <domluabind/capi.h>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
template<class T, bool = std::is_enum_v<T>> struct xb_base { using type = T; };
template<class T> struct xb_base<T, true> { using type = std::underlying_type_t<T>; };
template<class T> T xb_integer(lua_State* L, int i) {
  using B = typename xb_base<T>::type;
  static_assert(std::is_integral_v<B>);
  int ok = 0;
  if (lua_type(L, i) != LUA_TNUMBER) luaL_argerror(L, i, "expected integer");
  lua_Integer v = luaconv_integer(L, i, &ok);
  if (!ok) luaL_argerror(L, i, "expected integer");
  if constexpr (std::is_unsigned_v<B>) {
    if (v < 0 || static_cast<unsigned long long>(v) > std::numeric_limits<B>::max())
      luaL_argerror(L, i, "integer out of range");
  } else {
    if (v < std::numeric_limits<B>::lowest() || v > std::numeric_limits<B>::max())
      luaL_argerror(L, i, "integer out of range");
  }
  return static_cast<T>(v);
}
template<class T> T xb_number(lua_State* L, int i) {
  static_assert(std::is_floating_point_v<T>);
  if (lua_type(L, i) != LUA_TNUMBER) luaL_argerror(L, i, "expected number");
  int ok = 0;
  auto v = luaconv_number(L, i, &ok);
  if (!ok || (std::isfinite(v) && (v > std::numeric_limits<T>::max() || v < -std::numeric_limits<T>::max())))
    luaL_argerror(L, i, "number out of range");
  return static_cast<T>(v);
}
inline const char* xb_string(lua_State* L, int i, bool nullable) {
  if (nullable && lua_isnil(L, i)) return nullptr;
  if (lua_type(L, i) != LUA_TSTRING) luaL_argerror(L, i, "expected string");
  size_t n = 0;
  const char* s = luaconv_string(L, i, &n);
  if (std::memchr(s, 0, n)) luaL_argerror(L, i, "embedded NUL in C string");
  return s;
}
template<class T> T xb_pointer(lua_State* L, int i, bool nullable) {
  static_assert(std::is_pointer_v<T>);
  if (nullable && lua_isnil(L, i)) return nullptr;
  if (!lua_islightuserdata(L, i) || (!nullable && !lua_touserdata(L, i)))
    luaL_argerror(L, i, "expected non-null opaque pointer");
  return static_cast<T>(lua_touserdata(L, i));
}
inline bool xb_boolean(lua_State* L, int i) {
  if (!lua_isboolean(L, i)) luaL_argerror(L, i, "expected boolean");
  return luaconv_boolean(L, i) != 0;
}
template<class T> void xb_push_integer(lua_State* L, T value) {
  using B = typename xb_base<T>::type;
  static_assert(std::is_integral_v<B>);
  if constexpr (std::is_unsigned_v<B>) {
    if (static_cast<unsigned long long>(value) > static_cast<unsigned long long>(LUA_MAXINTEGER))
      luaL_error(L, "unsigned return exceeds Lua integer range");
  } else if constexpr (sizeof(B) > sizeof(lua_Integer)) {
    if (static_cast<B>(value) < LUA_MININTEGER || static_cast<B>(value) > LUA_MAXINTEGER)
      luaL_error(L, "signed return exceeds Lua integer range");
  }
  luaconv_push_integer(L, static_cast<lua_Integer>(value));
}
} // namespace
)CPP";
  for (size_t i = 0; i < m.functions.size(); ++i) {
    const auto& f = m.functions[i];
    out << "static int xb_fn_" << i << "(lua_State* L) {\n  if (lua_gettop(L) != " << f.params.size()
        << ") return luaL_error(L, \"expected " << f.params.size() << " arguments\");\n";
    for (size_t j = 0; j < f.params.size(); ++j) {
      const auto& p = f.params[j];
      auto [native, nullable] = lower(p.c_type, p.options, false);
      out << "  auto p" << j << " = ";
      if (native == "integer" || native == "number" || native == "lightuserdata")
        out << "xb_" << (native == "lightuserdata" ? "pointer" : native) << '<' << p.c_type << '>';
      else out << "xb_" << native;
      out << "(L, " << j + 1;
      if (native == "string" || native == "lightuserdata") out << ", " << (nullable ? "true" : "false");
      out << ");\n";
    }
    auto [native, nullable] = lower(f.returns, f.options, true);
    (void)nullable;
    out << "  ";
    if (native != "nil") out << "auto result = ";
    out << f.name << '(';
    for (size_t j = 0; j < f.params.size(); ++j) { if (j) out << ", "; out << 'p' << j; }
    out << ");\n";
    if (native == "integer") out << "  xb_push_integer(L, result);\n";
    else if (native == "number") out << "  luaconv_push_number(L, result);\n";
    else if (native == "boolean") out << "  luaconv_push_boolean(L, result);\n";
    else if (native == "string") out << "  if (result) luaconv_push_string(L, result, std::strlen(result)); else luaconv_push_nil(L);\n";
    else if (native == "lightuserdata") out << "  if (result) lua_pushlightuserdata(L, const_cast<void*>(static_cast<const void*>(result))); else luaconv_push_nil(L);\n";
    out << "  return " << (native == "nil" ? 0 : 1) << ";\n}\n";
  }
  out << "extern \"C\" int luaopen_" << module << "(lua_State* L) {\n  luatable_new(L, 0, " << m.functions.size() << ");\n";
  for (size_t i = 0; i < m.functions.size(); ++i)
    out << "  lua_pushcfunction(L, xb_fn_" << i << "); luatable_set(L, -2, " << quote(m.functions[i].name) << ");\n";
  out << "  return 1;\n}\n";
  return out.str();
}
} // namespace xbind::codegen
