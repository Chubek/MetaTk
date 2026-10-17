#include "qscript/src/ValueInternal.h"

namespace qscript {
namespace {
Result<Value> invalid(std::string message) {
    return {std::nullopt, {{metachain::Stage::Verify, std::move(message), {}, 0, 0, 0}}};
}
}
Value::Value(std::shared_ptr<const detail::HostValue> value) : data_(std::move(value)) {}
Value::Value() : Value(std::int64_t(0)) {}
Value::Value(std::int64_t integer) {
    auto value = std::make_shared<detail::HostValue>();
    value->integer = integer; data_ = std::move(value);
}
Value::Value(std::string text) {
    auto value = std::make_shared<detail::HostValue>();
    value->type = detail::Type::string(); value->spelling = "str";
    value->text = std::move(text); data_ = std::move(value);
}
Result<Value> Value::list(std::string_view element_type, std::vector<Value> items) {
    auto element = detail::parse_type(element_type);
    if (!element || !detail::parse_type("list<" + std::string(element_type) + ">"))
        return invalid("invalid list element type or type nesting limit exceeded");
    for (const auto& item : items)
        if (item.type() != element_type) return invalid("list element expects " + std::string(element_type) + ", got " + std::string(item.type()));
    auto value = std::make_shared<detail::HostValue>();
    value->type = detail::Type::list(std::move(*element)); value->spelling = value->type.str();
    value->items = std::move(items);
    return {Value(std::move(value)), {}};
}
Result<Value> Value::compile_grammar(std::string_view source, Options options) {
    if (source.size() > options.toolchain.il.max_source_bytes) return invalid("source size limit exceeded");
    auto compiled = qs_grammar::compile(source);
    if (!compiled.ok()) return invalid("invalid grammar: " + compiled.error);
    auto value = std::make_shared<detail::HostValue>();
    value->type = detail::Type::grammar(); value->spelling = "grammar";
    value->name = value->internal = compiled.name; value->source = source;
    value->grammar = std::make_shared<const qs_grammar>(std::move(compiled));
    return {Value(std::move(value)), {}};
}
Result<Value> Value::from_match(const qs_match& match, Options options) {
    // Validate before the recursive C++ copy, including host-created trees.
    std::vector<std::pair<const qs_match*, std::size_t>> pending{{&match, 1}};
    std::size_t nodes = 0, bytes = 0;
    while (!pending.empty()) {
        auto [node, depth] = pending.back(); pending.pop_back();
        if (depth > std::min<std::size_t>(options.max_depth, 256) || ++nodes > options.max_nodes)
            return invalid("match tree limit exceeded");
        for (const auto* text : {&node->name, &node->sym, &node->text}) {
            if (text->size() > options.max_heap_bytes - bytes) return invalid("heap size limit exceeded");
            bytes += text->size();
        }
        if (node->children.size() > options.max_nodes - nodes || pending.size() > options.max_nodes - nodes - node->children.size())
            return invalid("match tree limit exceeded");
        for (const auto& child : node->children) pending.emplace_back(&child, depth + 1);
    }
    auto value = std::make_shared<detail::HostValue>();
    value->type = detail::Type::match(); value->spelling = "match";
    value->root = std::make_shared<const qs_match>(match); value->node = value->root.get(); value->text = match.text;
    return {Value(std::move(value)), {}};
}
Value::Kind Value::kind() const noexcept { return data_ ? static_cast<Kind>(data_->type.kind) : Kind::Int; }
std::string_view Value::type() const noexcept { return data_ ? std::string_view(data_->spelling) : std::string_view("int"); }
std::optional<std::int64_t> Value::as_integer() const noexcept {
    return kind() == Kind::Int ? std::optional<std::int64_t>(data_ ? data_->integer : 0) : std::nullopt;
}
const std::string* Value::as_string() const noexcept { return kind() == Kind::Str ? &data_->text : nullptr; }
const std::vector<Value>* Value::as_list() const noexcept { return kind() == Kind::List ? &data_->items : nullptr; }
const qs_grammar* Value::as_grammar() const noexcept { return kind() == Kind::Grammar ? data_->grammar.get() : nullptr; }
const qs_match* Value::as_match() const noexcept { return kind() == Kind::Match ? data_->node : nullptr; }
std::string_view Value::match_error() const noexcept { return kind() == Kind::Match ? std::string_view(data_->error) : std::string_view{}; }
} // namespace qscript
