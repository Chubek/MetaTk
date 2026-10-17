#include "qscript/include/QScript-Runtime.h"
#include "qscript/src/Context.h"

namespace {
qscript::Result<qscript::Value> invalid(std::string message) {
    return {std::nullopt, {{metachain::Stage::Verify, std::move(message), {}, 0, 0, 0}}};
}
template<class Function> qs_value* create(qs_context* context, Function function) {
    qs_value* result = nullptr;
    qscript::detail::protect(context, [&] {
        auto value = function();
        if (!value) { qscript::detail::report(context, value.diagnostics); return QS_ERROR; }
        result = new qs_value{std::move(*value)};
        return QS_OK;
    });
    return result;
}
}
extern "C" {
qs_value* qs_value_int(qs_context* c, int64_t n) {
    return create(c, [&]() -> qscript::Result<qscript::Value> { return {qscript::Value(n), {}}; });
}
qs_value* qs_value_string(qs_context* c, const char* text, size_t length) {
    return create(c, [&]() -> qscript::Result<qscript::Value> {
        if (!text && length) return invalid("null string");
        if (length > c->options.max_heap_bytes) return invalid("heap size limit exceeded");
        return {qscript::Value(std::string(text ? text : "", length)), {}};
    });
}
qs_value* qs_value_list(qs_context* c, const char* type, const qs_value* const* items, size_t count) {
    return create(c, [&]() -> qscript::Result<qscript::Value> {
        if (!type || (!items && count)) return invalid("null list argument");
        if (count > c->options.max_heap_bytes / sizeof(int64_t)) return invalid("heap size limit exceeded");
        std::vector<qscript::Value> values;
        values.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            if (!items[i]) return invalid("null list element");
            values.push_back(items[i]->value);
        }
        return qscript::Value::list(type, std::move(values));
    });
}
qs_value* qs_value_grammar(qs_context* c, const char* source, size_t length) {
    return create(c, [&]() -> qscript::Result<qscript::Value> {
        if (!source && length) return invalid("null grammar source");
        return qscript::Value::compile_grammar(std::string_view(source ? source : "", length), c->options);
    });
}
qs_value* qs_value_match(qs_context* c, const qs_match* match) {
    return create(c, [&]() -> qscript::Result<qscript::Value> {
        if (!match) return invalid("null match");
        return qscript::Value::from_match(*match, c->options);
    });
}
qs_value* qs_value_clone(qs_context* c, const qs_value* value) {
    return create(c, [&]() -> qscript::Result<qscript::Value> {
        if (!value) return invalid("null value");
        return {value->value, {}};
    });
}
void qs_value_destroy(qs_value* value) { delete value; }
int qs_value_get_kind(const qs_value* value) { return value ? static_cast<int>(value->value.kind()) : -1; }
const char* qs_value_type(const qs_value* value) { return value ? value->value.type().data() : nullptr; }
qs_status qs_value_get_int(const qs_value* value, int64_t* result) {
    if (result) *result = 0;
    if (!value || !result) return QS_INVALID_ARGUMENT;
    const auto integer = value->value.as_integer();
    if (!integer) return QS_INVALID_ARGUMENT;
    *result = *integer; return QS_OK;
}
qs_status qs_value_get_string(const qs_value* value, const char** data, size_t* length) {
    if (data) *data = nullptr;
    if (length) *length = 0;
    if (!value || !data || !length) return QS_INVALID_ARGUMENT;
    const auto* text = value->value.as_string();
    if (!text) return QS_INVALID_ARGUMENT;
    *data = text->data(); *length = text->size(); return QS_OK;
}
size_t qs_value_list_size(const qs_value* value) {
    const auto* items = value ? value->value.as_list() : nullptr;
    return items ? items->size() : 0;
}
qs_value* qs_value_list_at(qs_context* c, const qs_value* value, size_t index) {
    return create(c, [&]() -> qscript::Result<qscript::Value> {
        const auto* items = value ? value->value.as_list() : nullptr;
        if (!items) return invalid("value is not a list");
        if (index >= items->size()) return invalid("list index out of range");
        return {(*items)[index], {}};
    });
}
const qs_grammar* qs_value_get_grammar(const qs_value* value) { return value ? value->value.as_grammar() : nullptr; }
const qs_match* qs_value_get_match(const qs_value* value) { return value ? value->value.as_match() : nullptr; }
const char* qs_value_match_error(const qs_value* value) {
    return value && value->value.kind() == qscript::Value::Kind::Match ? value->value.match_error().data() : nullptr;
}
}
