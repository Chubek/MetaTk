#ifndef METACHAIN_QSCRIPT_HOST_VALUE_H
#define METACHAIN_QSCRIPT_HOST_VALUE_H
#include "qscript/src/Runtime.h"

namespace qscript::detail {
struct HostValue {
    Type type;
    std::string spelling = "int";
    std::int64_t integer = 0;
    std::string text;
    std::vector<Value> items;
    std::shared_ptr<const qs_grammar> grammar;
    std::string name, internal, source;
    std::size_t depth = 0;
    std::shared_ptr<const qs_match> root;
    const qs_match* node = nullptr;
    std::string error;
    std::size_t line = 0, column = 0;
};
} // namespace qscript::detail
#endif
