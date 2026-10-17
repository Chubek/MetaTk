// QScript run-time heap and built-in operations (see Runtime.h).
#include "qscript/src/Runtime.h"
#include "qscript/src/ValueInternal.h"
#include "MetaChain.hpp"
#include <algorithm>
#include <functional>
#include <stdexcept>

namespace qscript::detail {
namespace {
const char* builtin_name(Builtin b) {
    switch (b) {
    case Builtin::Die: return "die";
    case Builtin::CompileGrammar: return "compile_grammar";
    default: return "builtin";
    }
}
} // namespace

// ---------------------------------------------------------------------------
// Resolution: which built-in implements `recv.name(...)` for a receiver type.
// ---------------------------------------------------------------------------
std::optional<BuiltinCall> resolve_method(const Type& recv, std::string_view name, std::size_t argc, std::string& why) {
    const Type I = Type::integer(), S = Type::string(), G = Type::grammar(), M = Type::match();
    // Methods take the receiver as their first parameter.
    auto method = [&](Builtin b, std::vector<Type> params, Type result, std::optional<Type> type_arg = std::nullopt) {
        params.insert(params.begin(), recv);
        return BuiltinCall{b, std::move(params), std::move(result), std::move(type_arg)};
    };
    auto match = [&](std::size_t n) { return argc == n; };
    std::optional<BuiltinCall> none;

    switch (recv.kind) {
    case Type::Kind::Int:
        if (name == "str" && match(0)) return method(Builtin::IntToStr, {}, S);
        if (name == "chr" && match(0)) return method(Builtin::IntToChar, {}, S);
        break;
    case Type::Kind::Str:
        if (name == "len" && match(0)) return method(Builtin::StrLen, {}, I);
        if (name == "at" && match(1)) return method(Builtin::StrAt, {I}, I);
        if (name == "sub" && match(2)) return method(Builtin::StrSub, {I, I}, S);
        if (name == "find" && match(1)) return method(Builtin::StrFind, {S}, I);
        if (name == "find" && match(2)) return method(Builtin::StrFindFrom, {S, I}, I);
        if (name == "starts_with" && match(1)) return method(Builtin::StrStarts, {S}, I);
        if (name == "ends_with" && match(1)) return method(Builtin::StrEnds, {S}, I);
        if (name == "contains" && match(1)) return method(Builtin::StrContains, {S}, I);
        if (name == "split" && match(1)) return method(Builtin::StrSplit, {S}, Type::list(S));
        if (name == "repeat" && match(1)) return method(Builtin::StrRepeat, {I}, S);
        if (name == "replace" && match(2)) return method(Builtin::StrReplace, {S, S}, S);
        if (name == "to_int" && match(0)) return method(Builtin::StrToInt, {}, I);
        if (name == "upper" && match(0)) return method(Builtin::StrUpper, {}, S);
        if (name == "lower" && match(0)) return method(Builtin::StrLower, {}, S);
        if (name == "trim" && match(0)) return method(Builtin::StrTrim, {}, S);
        break;
    case Type::Kind::List: {
        const Type& e = recv.element();
        const bool comparable = e.is_int() || e.is_str();
        if (name == "len" && match(0)) return method(Builtin::ListLen, {}, I);
        if (name == "push" && match(1)) return method(Builtin::ListPush, {e}, I);
        if (name == "pop" && match(0)) return method(Builtin::ListPop, {}, e);
        if (name == "last" && match(0)) return method(Builtin::ListLast, {}, e);
        if (name == "clear" && match(0)) return method(Builtin::ListClear, {}, I);
        if (name == "reverse" && match(0)) return method(Builtin::ListReverse, {}, I);
        if (name == "slice" && match(2)) return method(Builtin::ListSlice, {I, I}, recv);
        if (name == "contains" && match(1)) {
            if (!comparable) { why = "contains needs a list of int or str, not " + recv.str(); return none; }
            return method(Builtin::ListContains, {e}, I, e);
        }
        if (name == "sort" && match(0)) {
            if (!comparable) { why = "sort needs a list of int or str, not " + recv.str(); return none; }
            return method(Builtin::ListSort, {}, I, e);
        }
        if (name == "join" && match(1)) {
            if (!e.is_str()) { why = "join needs a list<str>, not " + recv.str(); return none; }
            return method(Builtin::ListJoin, {S}, S);
        }
        break;
    }
    case Type::Kind::Grammar:
        if (name == "parse" && match(1)) return method(Builtin::GrammarParse, {S}, M);
        if (name == "parse" && match(2)) return method(Builtin::GrammarParseRule, {S, S}, M);
        if (name == "name" && match(0)) return method(Builtin::GrammarName, {}, S);
        if (name == "source" && match(0)) return method(Builtin::GrammarSource, {}, S);
        if (name == "rules" && match(0)) return method(Builtin::GrammarRules, {}, Type::list(S));
        if (name == "has_rule" && match(1)) return method(Builtin::GrammarHasRule, {S}, I);
        if (name == "extend" && match(1)) return method(Builtin::GrammarExtend, {S}, G);
        break;
    case Type::Kind::Match:
        if (name == "ok" && match(0)) return method(Builtin::MatchOk, {}, I);
        if (name == "text" && match(0)) return method(Builtin::MatchText, {}, S);
        if (name == "rule" && match(0)) return method(Builtin::MatchRule, {}, S);
        if (name == "sym" && match(0)) return method(Builtin::MatchSym, {}, S);
        if (name == "from" && match(0)) return method(Builtin::MatchFrom, {}, I);
        if (name == "to" && match(0)) return method(Builtin::MatchTo, {}, I);
        if (name == "line" && match(0)) return method(Builtin::MatchLine, {}, I);
        if (name == "column" && match(0)) return method(Builtin::MatchColumn, {}, I);
        if (name == "size" && match(0)) return method(Builtin::MatchSize, {}, I);
        if (name == "at" && match(1)) return method(Builtin::MatchAt, {I}, M);
        if (name == "has" && match(1)) return method(Builtin::MatchHas, {S}, I);
        if (name == "count" && match(1)) return method(Builtin::MatchCount, {S}, I);
        if (name == "child" && match(1)) return method(Builtin::MatchChild, {S}, M);
        if (name == "nth" && match(2)) return method(Builtin::MatchNth, {S, I}, M);
        if (name == "dump" && match(0)) return method(Builtin::MatchDump, {}, S);
        if (name == "error" && match(0)) return method(Builtin::MatchError, {}, S);
        if (name == "find_all" && match(1)) return method(Builtin::MatchFindAll, {S}, Type::list(M));
        if (name == "children" && match(1)) return method(Builtin::MatchChildren, {S}, Type::list(M));
        break;
    }
    if (why.empty()) why = "no method " + std::string(name) + " with " + std::to_string(argc) + " argument" + (argc == 1 ? "" : "s") + " on " + recv.str();
    return none;
}

std::optional<BuiltinCall> resolve_global(std::string_view name, std::size_t argc) {
    if (name == "die" && argc == 1) return BuiltinCall{Builtin::Die, {Type::string()}, Type::integer(), std::nullopt};
    if (name == "compile_grammar" && argc == 1) return BuiltinCall{Builtin::CompileGrammar, {Type::string()}, Type::grammar(), std::nullopt};
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Session
// ---------------------------------------------------------------------------
namespace {
constexpr std::size_t object_overhead = 64;
}

Session::Session(const Unit& unit, const Options& options) : unit_(unit), options_(options) {
    heap_.emplace_back();  // handle 0 is never valid
    string_handles_.assign(unit.strings.size(), 0);
    grammar_handles_.assign(unit.grammars.size(), 0);
}

std::int64_t Session::charge_bytes(std::size_t bytes) {
    if (bytes > options_.max_heap_bytes || bytes_ > options_.max_heap_bytes - bytes)
        throw std::runtime_error("heap size limit exceeded");
    bytes_ += bytes;
    return 0;
}

std::int64_t Session::allocate(Object object, std::size_t bytes) {
    if (heap_.size() > options_.max_heap_objects) throw std::runtime_error("heap object limit exceeded");
    if (bytes > SIZE_MAX - object_overhead) throw std::runtime_error("heap size limit exceeded");
    charge_bytes(bytes + object_overhead);
    heap_.push_back(std::move(object));
    return static_cast<std::int64_t>(heap_.size() - 1);
}

std::int64_t Session::import_value(const Value& value) {
    if (auto integer = value.as_integer()) return *integer;
    const auto& v = *value.data_;
    if (auto found = imported_.find(&v); found != imported_.end()) return found->second;
    Object object;
    object.kind = v.type.kind;
    object.text = v.text; object.grammar = v.grammar;
    object.name = v.name; object.internal = v.internal; object.source = v.source; object.depth = v.depth;
    object.root = v.root; object.node = v.node; object.error = v.error;
    object.line = v.line; object.column = v.column;
    std::size_t bytes = v.text.size();
    if (v.type.kind == Type::Kind::Grammar) {
        if (v.source.size() > SIZE_MAX - v.name.size()) throw std::runtime_error("heap size limit exceeded");
        bytes = v.source.size() + v.name.size();
    } else if (v.type.kind == Type::Kind::Match) {
        if (std::max(v.text.size(), v.error.size()) > SIZE_MAX - 64) throw std::runtime_error("heap size limit exceeded");
        bytes = std::max(v.text.size(), v.error.size()) + 64;
    }
    const auto handle = allocate(std::move(object), bytes);
    imported_.emplace(&v, handle);
    for (const auto& item : v.items) {
        const auto child = import_value(item);
        charge_bytes(8);
        this->object(handle, Type::Kind::List).items.push_back(child);
    }
    return handle;
}

Value Session::export_value(std::int64_t handle, const Type& type) {
    if (type.is_int()) return Value(handle);
    const auto key = std::make_pair(handle, type.str());
    if (auto found = exported_.find(key); found != exported_.end()) return found->second;
    // Copy before recursing: export constructs host allocations, never runtime
    // handles or references whose lifetime depends on this Session.
    const auto object = this->object(handle, type.kind);
    auto value = std::make_shared<HostValue>();
    value->type = type; value->spelling = key.second; value->text = object.text;
    value->grammar = object.grammar; value->name = object.name; value->internal = object.internal;
    value->source = object.source; value->depth = object.depth;
    value->root = object.root; value->node = object.node; value->error = object.error;
    value->line = object.line; value->column = object.column;
    if (type.is_list()) {
        value->items.reserve(object.items.size());
        for (auto item : object.items) value->items.push_back(export_value(item, type.element()));
    }
    Value result(std::move(value));
    exported_.emplace(key, result);
    return result;
}

Session::Object& Session::object(std::int64_t handle, Type::Kind kind) {
    if (handle <= 0 || static_cast<std::size_t>(handle) >= heap_.size()) throw std::runtime_error("internal: invalid QScript value");
    auto& o = heap_[static_cast<std::size_t>(handle)];
    if (o.kind != kind) throw std::runtime_error("internal: QScript value has the wrong type");
    return o;
}
const Session::Object& Session::object(std::int64_t handle, Type::Kind kind) const {
    return const_cast<Session*>(this)->object(handle, kind);
}

std::int64_t Session::make_string(std::string text) {
    const auto bytes = text.size();
    Object o; o.kind = Type::Kind::Str; o.text = std::move(text);
    return allocate(std::move(o), bytes);
}

std::int64_t Session::make_grammar(std::string name, std::string internal, std::string source, std::size_t depth,
                                   std::shared_ptr<const qs_grammar> compiled) {
    const auto bytes = source.size() + name.size();
    Object o; o.kind = Type::Kind::Grammar; o.name = std::move(name); o.internal = std::move(internal);
    o.source = std::move(source); o.depth = depth; o.grammar = std::move(compiled);
    return allocate(std::move(o), bytes);
}

std::int64_t Session::make_match(std::shared_ptr<const qs_match> root, const qs_match* node, std::string text) {
    const auto bytes = text.size() + 64;
    Object o; o.kind = Type::Kind::Match; o.text = std::move(text); o.root = std::move(root); o.node = node;
    return allocate(std::move(o), bytes);
}

std::int64_t Session::make_failed_match(std::string error, std::size_t line, std::size_t column) {
    Object o; o.kind = Type::Kind::Match; o.error = std::move(error); o.line = line; o.column = column;
    const auto bytes = o.error.size() + 64;
    return allocate(std::move(o), bytes);
}

std::shared_ptr<const qs_grammar> Session::compile_or_throw(const std::string& source) const {
    auto compiled = qs_grammar::compile(source);
    if (!compiled.ok()) throw std::runtime_error("invalid grammar: " + compiled.error);
    return std::make_shared<const qs_grammar>(std::move(compiled));
}

std::int64_t Session::string_literal(std::size_t index) {
    if (index >= unit_.strings.size()) throw std::runtime_error("internal: string literal out of range");
    auto& cached = string_handles_[index];
    if (!cached) cached = make_string(unit_.strings[index]);
    return cached;
}

std::int64_t Session::grammar_literal(std::size_t index) {
    if (index >= unit_.grammars.size()) throw std::runtime_error("internal: grammar out of range");
    auto& cached = grammar_handles_[index];
    if (cached) return cached;
    const auto& info = unit_.grammars[index];
    auto compiled = info.compiled ? info.compiled : compile_or_throw(info.source);
    cached = make_grammar(info.name, compiled->name, info.source, 0, compiled);
    return cached;
}

Type Session::type_of(std::int64_t descriptor) const {
    if (descriptor < 0 || static_cast<std::size_t>(descriptor) >= unit_.types.size()) throw std::runtime_error("internal: type descriptor out of range");
    auto t = parse_type(unit_.types[static_cast<std::size_t>(descriptor)]);
    if (!t) throw std::runtime_error("internal: invalid type descriptor");
    return *t;
}

void Session::emit_line(std::string_view text) {
    if (!options_.text_output)
        throw std::runtime_error("printing a string, list, grammar or match needs a text output (qs_set_text_output)");
    options_.text_output(text);
}

void Session::format_into(std::string& out, std::int64_t value, const Type& t, bool quote) {
    switch (t.kind) {
    case Type::Kind::Int: out += std::to_string(value); return;
    case Type::Kind::Str:
        if (quote) out += '"';
        out += object(value, Type::Kind::Str).text;
        if (quote) out += '"';
        return;
    case Type::Kind::Grammar: out += "grammar " + object(value, Type::Kind::Grammar).name; return;
    case Type::Kind::Match: {
        const auto& m = object(value, Type::Kind::Match);
        if (!m.node) { out += "(no match: " + m.error + ")"; return; }
        out += m.node->dump();
        return;
    }
    case Type::Kind::List: {
        const auto items = object(value, Type::Kind::List).items;  // copy: formatting does not allocate
        out += '[';
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i) out += ", ";
            format_into(out, items[i], t.element(), true);
        }
        out += ']';
        return;
    }
    }
}

std::string Session::format(std::int64_t value, const Type& t) {
    std::string out;
    format_into(out, value, t, false);
    return out;
}

std::int64_t Session::parse(std::int64_t gh, std::int64_t input_handle, const std::string& rule) {
    const auto& g = object(gh, Type::Kind::Grammar);
    const std::string input = text(input_handle);
    const auto names = g.grammar->rule_names();
    if (std::find(names.begin(), names.end(), rule) == names.end())
        throw std::runtime_error("grammar " + g.name + " has no rule " + rule);
    auto outcome = g.grammar->parse(input, rule);
    if (!outcome) return make_failed_match(outcome.error, outcome.line, outcome.column);
    auto root = std::make_shared<const qs_match>(std::move(*outcome.match));
    const qs_match* node = root.get();
    return make_match(root, node, node->text);
}

std::int64_t Session::dispatch(std::int64_t id, std::span<const std::int64_t> regs) {
    const auto b = static_cast<Builtin>(id - builtin_base);
    auto arg = [&](std::size_t i) { return regs[i]; };
    auto index_check = [](std::int64_t i, std::size_t n) {
        if (i < 0 || static_cast<std::uint64_t>(i) >= n)
            throw std::runtime_error("index " + std::to_string(i) + " out of range (length " + std::to_string(n) + ")");
    };
    switch (b) {
    case Builtin::Die: {
        const auto& message = text(arg(0));
        throw std::runtime_error(message.empty() ? std::string("die") : message);
    }
    case Builtin::CompileGrammar: {
        const std::string source = text(arg(0));
        auto compiled = compile_or_throw(source);
        const auto name = compiled->name;
        return make_grammar(name, name, source, 0, compiled);
    }
    case Builtin::StrLiteral: return string_literal(static_cast<std::size_t>(arg(0)));
    case Builtin::GrammarRef: return grammar_literal(static_cast<std::size_t>(arg(0)));
    case Builtin::ListNew: {
        Object o; o.kind = Type::Kind::List;
        return allocate(std::move(o), 0);
    }
    case Builtin::ListPush: {
        object(arg(0), Type::Kind::List);
        charge_bytes(8);
        object(arg(0), Type::Kind::List).items.push_back(arg(1));
        return 0;
    }
    case Builtin::ListGet: {
        const auto& items = object(arg(0), Type::Kind::List).items;
        index_check(arg(1), items.size());
        return items[static_cast<std::size_t>(arg(1))];
    }
    case Builtin::ListSet: {
        auto& items = object(arg(0), Type::Kind::List).items;
        index_check(arg(1), items.size());
        items[static_cast<std::size_t>(arg(1))] = arg(2);
        return 0;
    }
    case Builtin::ListLen: return static_cast<std::int64_t>(object(arg(0), Type::Kind::List).items.size());
    case Builtin::ListPop: {
        auto& items = object(arg(0), Type::Kind::List).items;
        if (items.empty()) throw std::runtime_error("pop from an empty list");
        const auto v = items.back(); items.pop_back();
        return v;
    }
    case Builtin::ListLast: {
        const auto& items = object(arg(0), Type::Kind::List).items;
        if (items.empty()) throw std::runtime_error("last of an empty list");
        return items.back();
    }
    case Builtin::ListContains: {
        const auto t = type_of(regs[2]);  // the element type
        const auto& items = object(arg(0), Type::Kind::List).items;
        for (auto v : items) {
            if (t.is_int() ? v == arg(1) : text(v) == text(arg(1))) return 1;
        }
        return 0;
    }
    case Builtin::ListSort: {
        const auto t = type_of(regs[1]);  // the element type
        auto& items = object(arg(0), Type::Kind::List).items;
        if (t.is_int()) std::sort(items.begin(), items.end());
        else {
            // Sort by text; keep handles in place of their strings.
            std::vector<std::int64_t> sorted = items;
            std::stable_sort(sorted.begin(), sorted.end(), [&](std::int64_t a, std::int64_t b) { return text(a) < text(b); });
            object(arg(0), Type::Kind::List).items = std::move(sorted);
        }
        return 0;
    }
    case Builtin::ListReverse: {
        auto& items = object(arg(0), Type::Kind::List).items;
        std::reverse(items.begin(), items.end());
        return 0;
    }
    case Builtin::ListClear: object(arg(0), Type::Kind::List).items.clear(); return 0;
    case Builtin::ListJoin: {
        const auto items = object(arg(0), Type::Kind::List).items;
        const std::string separator = text(arg(1));
        std::string out;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i) out += separator;
            out += text(items[i]);
        }
        return make_string(std::move(out));
    }
    case Builtin::ListSlice: {
        const auto items = object(arg(0), Type::Kind::List).items;
        const auto from = arg(1), count = arg(2);
        if (from < 0 || count < 0 || static_cast<std::uint64_t>(from) > items.size() ||
            static_cast<std::uint64_t>(count) > items.size() - static_cast<std::size_t>(from))
            throw std::runtime_error("slice out of range");
        Object o; o.kind = Type::Kind::List;
        o.items.assign(items.begin() + from, items.begin() + from + count);
        return allocate(std::move(o), static_cast<std::size_t>(count) * 8);
    }
    case Builtin::StrCompare: {
        const int c = text(arg(0)).compare(text(arg(1)));
        return c < 0 ? -1 : c > 0 ? 1 : 0;
    }
    case Builtin::StrConcat: return make_string(text(arg(0)) + text(arg(1)));
    case Builtin::StrLen: return static_cast<std::int64_t>(text(arg(0)).size());
    case Builtin::StrAt: {
        const auto& s = text(arg(0));
        index_check(arg(1), s.size());
        return static_cast<unsigned char>(s[static_cast<std::size_t>(arg(1))]);
    }
    case Builtin::StrSub: {
        const auto s = text(arg(0));
        const auto from = arg(1), count = arg(2);
        if (from < 0 || count < 0 || static_cast<std::uint64_t>(from) > s.size() || static_cast<std::uint64_t>(count) > s.size() - static_cast<std::size_t>(from))
            throw std::runtime_error("substring out of range");
        return make_string(s.substr(static_cast<std::size_t>(from), static_cast<std::size_t>(count)));
    }
    case Builtin::StrFind: {
        auto p = text(arg(0)).find(text(arg(1)));
        return p == std::string::npos ? -1 : static_cast<std::int64_t>(p);
    }
    case Builtin::StrFindFrom: {
        const auto& s = text(arg(0));
        if (arg(2) < 0 || static_cast<std::uint64_t>(arg(2)) > s.size()) throw std::runtime_error("search position out of range");
        auto p = s.find(text(arg(1)), static_cast<std::size_t>(arg(2)));
        return p == std::string::npos ? -1 : static_cast<std::int64_t>(p);
    }
    case Builtin::StrStarts: {
        const auto& s = text(arg(0)); const auto& p = text(arg(1));
        return bool_value(s.size() >= p.size() && s.compare(0, p.size(), p) == 0);
    }
    case Builtin::StrEnds: {
        const auto& s = text(arg(0)); const auto& p = text(arg(1));
        return bool_value(s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0);
    }
    case Builtin::StrContains: return bool_value(text(arg(0)).find(text(arg(1))) != std::string::npos);
    case Builtin::StrSplit: {
        const auto s = text(arg(0)), sep = text(arg(1));
        if (sep.empty()) throw std::runtime_error("split separator must not be empty");
        Object o; o.kind = Type::Kind::List;
        std::size_t start = 0;
        std::vector<std::int64_t> parts;
        for (;;) {
            auto p = s.find(sep, start);
            parts.push_back(make_string(s.substr(start, p == std::string::npos ? std::string::npos : p - start)));
            if (p == std::string::npos) break;
            start = p + sep.size();
        }
        const auto bytes = 8 * parts.size();
        o.items = std::move(parts);
        return allocate(std::move(o), bytes);
    }
    case Builtin::StrRepeat: {
        const auto& s = text(arg(0));
        const auto n = arg(1);
        if (n < 0) throw std::runtime_error("repeat count must not be negative");
        if (n > 0 && s.size() > options_.max_heap_bytes / static_cast<std::size_t>(n)) throw std::runtime_error("heap size limit exceeded");
        std::string out;
        out.reserve(s.size() * static_cast<std::size_t>(n));
        for (std::int64_t i = 0; i < n; ++i) out += s;
        return make_string(std::move(out));
    }
    case Builtin::StrToInt: {
        const auto& s = text(arg(0));
        auto value = metachain::text::signed_integer(s);
        if (!value) throw std::runtime_error("not a signed 64-bit integer: \"" + s + "\"");
        return *value;
    }
    case Builtin::StrUpper: case Builtin::StrLower: {
        std::string s = text(arg(0));
        for (auto& c : s) c = b == Builtin::StrUpper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
                                                       : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return make_string(std::move(s));
    }
    case Builtin::StrTrim: {
        const auto& s = text(arg(0));
        const auto first = s.find_first_not_of(" \t\r\n\f\v");
        if (first == std::string::npos) return make_string({});
        const auto last = s.find_last_not_of(" \t\r\n\f\v");
        return make_string(s.substr(first, last - first + 1));
    }
    case Builtin::StrReplace: {
        const auto s = text(arg(0)), from = text(arg(1)), to = text(arg(2));
        if (from.empty()) throw std::runtime_error("replace pattern must not be empty");
        std::string out;
        std::size_t start = 0;
        for (;;) {
            auto p = s.find(from, start);
            if (p == std::string::npos) { out += s.substr(start); break; }
            out += s.substr(start, p - start);
            out += to;
            start = p + from.size();
        }
        return make_string(std::move(out));
    }
    case Builtin::IntToStr: return make_string(std::to_string(arg(0)));
    case Builtin::IntToChar: {
        if (arg(0) < 0 || arg(0) > 255) throw std::runtime_error("character code out of range 0..255");
        return make_string(std::string(1, static_cast<char>(arg(0))));
    }
    case Builtin::GrammarParse: return parse(arg(0), arg(1), "TOP");
    case Builtin::GrammarParseRule: return parse(arg(0), arg(1), text(arg(2)));
    case Builtin::GrammarName: return make_string(object(arg(0), Type::Kind::Grammar).name);
    case Builtin::GrammarSource: return make_string(object(arg(0), Type::Kind::Grammar).source);
    case Builtin::GrammarRules: {
        const auto names = object(arg(0), Type::Kind::Grammar).grammar->rule_names();
        Object o; o.kind = Type::Kind::List;
        for (const auto& n : names) o.items.push_back(make_string(n));
        return allocate(std::move(o), 8 * names.size());
    }
    case Builtin::GrammarHasRule: {
        const auto names = object(arg(0), Type::Kind::Grammar).grammar->rule_names();
        return bool_value(std::find(names.begin(), names.end(), text(arg(1))) != names.end());
    }
    case Builtin::GrammarExtend: {
        const auto base = object(arg(0), Type::Kind::Grammar);  // copy: allocation may move the heap
        const std::string body = text(arg(1));
        const std::string internal = "__qs_ext" + std::to_string(base.depth + 1);
        std::string source = base.source + "\ngrammar " + internal + " is " + base.internal + " {" + body + "}";
        auto compiled = compile_or_throw(source);
        return make_grammar(base.name, internal, std::move(source), base.depth + 1, compiled);
    }
    case Builtin::MatchOk: return bool_value(object(arg(0), Type::Kind::Match).node != nullptr);
    case Builtin::MatchText: return make_string(object(arg(0), Type::Kind::Match).text);
    case Builtin::MatchRule: {
        const auto* n = object(arg(0), Type::Kind::Match).node;
        return make_string(n ? n->name : std::string());
    }
    case Builtin::MatchSym: {
        const auto* n = object(arg(0), Type::Kind::Match).node;
        return make_string(n ? n->sym : std::string());
    }
    case Builtin::MatchError: return make_string(object(arg(0), Type::Kind::Match).error);
    case Builtin::MatchFrom: { const auto* n = object(arg(0), Type::Kind::Match).node; return n ? static_cast<std::int64_t>(n->from) : 0; }
    case Builtin::MatchTo: { const auto* n = object(arg(0), Type::Kind::Match).node; return n ? static_cast<std::int64_t>(n->to) : 0; }
    case Builtin::MatchLine: {
        const auto& m = object(arg(0), Type::Kind::Match);
        return static_cast<std::int64_t>(m.node ? m.node->line : m.line);
    }
    case Builtin::MatchColumn: {
        const auto& m = object(arg(0), Type::Kind::Match);
        return static_cast<std::int64_t>(m.node ? m.node->column : m.column);
    }
    case Builtin::MatchSize: {
        const auto* n = object(arg(0), Type::Kind::Match).node;
        return n ? static_cast<std::int64_t>(n->children.size()) : 0;
    }
    case Builtin::MatchAt: {
        const auto& m = object(arg(0), Type::Kind::Match);
        if (!m.node) throw std::runtime_error("at() on a failed match");
        index_check(arg(1), m.node->children.size());
        const auto& child = m.node->children[static_cast<std::size_t>(arg(1))];
        return make_match(m.root, &child, child.text);
    }
    case Builtin::MatchHas: {
        const auto& m = object(arg(0), Type::Kind::Match);
        return bool_value(m.node && m.node->find(text(arg(1))) != nullptr);
    }
    case Builtin::MatchCount: {
        const auto& m = object(arg(0), Type::Kind::Match);
        if (!m.node) return 0;
        const auto& name = text(arg(1));
        if (name == "*") return static_cast<std::int64_t>(m.node->children.size());
        return static_cast<std::int64_t>(m.node->all(name).size());
    }
    case Builtin::MatchChildren: {
        const auto m = object(arg(0), Type::Kind::Match);  // copy: allocation below
        const auto name = text(arg(1));
        std::vector<std::int64_t> items;
        if (m.node)
            for (const auto& child : m.node->children)
                if (child.name == name) items.push_back(make_match(m.root, &child, child.text));
        Object list; list.kind = Type::Kind::List; list.items = std::move(items);
        const auto bytes = list.items.size() * sizeof(std::int64_t);
        return allocate(std::move(list), bytes);
    }
    case Builtin::MatchFindAll: {
        const auto m = object(arg(0), Type::Kind::Match);  // copy: allocation below
        const auto name = text(arg(1));
        std::vector<std::int64_t> items;
        if (m.node) {
            // Pre-order over the descendants of the matched node.
            std::function<void(const qs_match&)> walk = [&](const qs_match& node) {
                for (const auto& child : node.children) {
                    if (child.name == name) items.push_back(make_match(m.root, &child, child.text));
                    walk(child);
                }
            };
            walk(*m.node);
        }
        Object list; list.kind = Type::Kind::List; list.items = std::move(items);
        const auto bytes = list.items.size() * sizeof(std::int64_t);
        return allocate(std::move(list), bytes);
    }
    case Builtin::MatchChild: case Builtin::MatchNth: {
        const auto m = object(arg(0), Type::Kind::Match);  // copy: allocation below
        const auto name = text(arg(1));
        if (!m.node) throw std::runtime_error("child of a failed match");
        const auto matches = m.node->all(name);
        const std::size_t which = b == Builtin::MatchNth ? static_cast<std::size_t>(arg(2)) : 0;
        if (b == Builtin::MatchNth && (arg(2) < 0)) throw std::runtime_error("capture index out of range");
        if (which >= matches.size()) throw std::runtime_error("match has no capture " + name + (b == Builtin::MatchNth ? "[" + std::to_string(arg(2)) + "]" : std::string()));
        return make_match(m.root, matches[which], matches[which]->text);
    }
    case Builtin::MatchDump: {
        const auto& m = object(arg(0), Type::Kind::Match);
        return make_string(m.node ? m.node->dump() : "(no match: " + m.error + ")");
    }
    case Builtin::PrintValue: {
        const auto t = type_of(arg(1));
        // Without a text output, integers are kept for Execution::output.
        if (t.is_int() && !options_.text_output) { ints_.push_back(arg(0)); return 0; }
        emit_line(format(arg(0), t));
        return 0;
    }
    }
    throw std::runtime_error(std::string("internal: unknown built-in ") + builtin_name(b));
}
} // namespace qscript::detail
