#ifndef METACHAIN_QSCRIPT_TYPES_H
#define METACHAIN_QSCRIPT_TYPES_H
// QScript static types (not installed API).
//
//   int            signed 64-bit integer (also booleans and characters)
//   str            immutable byte string
//   list<T>        growable list of T (T is any type)
//   grammar        a compiled grammar (first-class, see manual/qscript/05)
//   match          a parse result: a node of a match tree, or a failure
//
// At run time every value is one 64-bit register. Integers are the value
// itself; the other types are handles into the run's heap (Runtime.h). The
// compiler knows each expression's type, so a handle is never read as an
// integer and an integer never as a handle.
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace qscript::detail {
struct Type {
    enum class Kind : std::uint8_t { Int, Str, List, Grammar, Match };
    Kind kind = Kind::Int;
    std::shared_ptr<const Type> elem;  // List only

    static Type integer() { return Type{}; }
    static Type string() { Type t; t.kind = Kind::Str; return t; }
    static Type grammar() { Type t; t.kind = Kind::Grammar; return t; }
    static Type match() { Type t; t.kind = Kind::Match; return t; }
    static Type list(Type e) { Type t; t.kind = Kind::List; t.elem = std::make_shared<const Type>(std::move(e)); return t; }

    bool is_int() const { return kind == Kind::Int; }
    bool is_str() const { return kind == Kind::Str; }
    bool is_list() const { return kind == Kind::List; }
    bool is_handle() const { return kind != Kind::Int; }
    // Element type of a list (requires is_list()).
    const Type& element() const { return *elem; }

    bool operator==(const Type& o) const {
        if (kind != o.kind) return false;
        if (kind != Kind::List) return true;
        return *elem == *o.elem;
    }
    bool operator!=(const Type& o) const { return !(*this == o); }

    // Canonical spelling: int, str, grammar, match, list<T>.
    std::string str() const {
        switch (kind) {
        case Kind::Int: return "int";
        case Kind::Str: return "str";
        case Kind::Grammar: return "grammar";
        case Kind::Match: return "match";
        case Kind::List: return "list<" + elem->str() + ">";
        }
        return "int";
    }
};

// Parse a canonical or annotated type spelling ("list<str>"). Whitespace is
// not allowed; the parser already normalises annotations.
inline std::optional<Type> parse_type(std::string_view text) {
    std::size_t depth = 0;
    while (text.size() > 6 && text.starts_with("list<") && text.back() == '>') {
        if (++depth >= 256) return std::nullopt;
        text = text.substr(5, text.size() - 6);
    }
    std::optional<Type> type;
    if (text == "int") type = Type::integer();
    if (text == "str") type = Type::string();
    if (text == "grammar") type = Type::grammar();
    if (text == "match") type = Type::match();
    if (!type) return std::nullopt;
    while (depth--) *type = Type::list(std::move(*type));
    return type;
}
} // namespace qscript::detail
#endif
