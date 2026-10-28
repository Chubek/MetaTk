#pragma once
#include <juniper/Juniper.hpp>
#include <deque>

namespace juniper::detail {
inline Diagnostic error(std::string code, std::string message, std::string file = {}) {
    return {std::move(code), std::move(file), std::move(message), 0, 0};
}
bool identifier(std::string_view);
Result<bool> validate_backend(const Backend&);
Result<bool> validate_structure(const Module&);
std::string emit_backend(const Backend&);
std::string quote(std::string_view);
Module import_module(const metail_module&, Backend);

// Stable backing storage for a borrowed MetaIL descriptor graph. Public
// structured entry points are used for validation, emission and optimization.
struct Bridge {
    std::deque<metail_expr> expressions;
    std::deque<std::vector<const metail_expr*>> arguments;
    std::vector<std::vector<const char*>> params;
    std::vector<std::vector<metail_stmt>> statements;
    std::vector<metail_function> functions;
    metail_module module{};
    const metail_expr* expression(const Expr&);
    explicit Bridge(const Module&);
};
Result<std::pair<Backend, std::string>> extract_backend(std::string_view, const std::string&);
} // namespace juniper::detail
