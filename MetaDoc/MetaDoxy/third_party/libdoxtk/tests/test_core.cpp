#include "doxtk/DoxTk.hpp"
#include "test_support.hpp"
#include <string_view>

int main() {
    using namespace doxtk;
    CHECK(error_code_name(ErrorCode::ok) == "ok");
    CHECK(error_code_name(ErrorCode::unsupported_conversion) == "unsupported_conversion");
    const auto result = Result<int>::success(42);
    CHECK(result.has_value());
    CHECK(*result.value == 42);
    const auto failure = Result<int>::failure(ErrorCode::parse_error, "bad syntax");
    CHECK(!failure);
    CHECK(failure.error.code == ErrorCode::parse_error);
    CHECK(!Result<int>{});
    const Result<int> inconsistent{42, {ErrorCode::parse_error, "bad", "test"}};
    CHECK(!inconsistent);
    CHECK(error_code_name(static_cast<ErrorCode>(999)) == "unknown");
    return 0;
}
