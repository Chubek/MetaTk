#include "all.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace metanum;
using namespace metanum::catalog;

// ---- compile-time catalog checks -------------------------------------------
static_assert(all_categories.size() == 40);
static_assert(total_operations == 298);
static_assert(manifest_version == 1);
static_assert(find_category("gsl.fft") == &gsl_fft_category);
static_assert(find_category("nope") == nullptr);
static_assert(find_operation("gsl.fft", "real_fft") == &gsl_fft[1]);
static_assert(find_operation("gsl.fft", "missing") == nullptr);
static_assert(symbol_count(gsl_fft[0]) == 5);
static_assert(symbol_at(gsl_fft[0], 2) == "gsl_fft_complex_forward");
static_assert(symbol_at(gsl_fft[0], 99).empty());
static_assert(references_symbol(gsl_fft[2], "gsl_fft_real_wavetable_alloc"));
static_assert(!references_symbol(gsl_fft[2], "gsl_fft_real"));
// paren-aware splitting
static_assert(symbol_count(*find(ppl_polyhedra_queries, "maximize")) == 1);
static_assert(symbol_count(*find(ppl_space_dimensions_and_expressions, "constraint")) == 1);
// '/' separator
static_assert(symbol_count(isl_basic_objects[6]) == 2);
static_assert(symbol_at(isl_basic_objects[6], 1) == "isl_map_dim");
static_assert(gsl_minimization_category.lib == library::gsl);
static_assert(to_string(library::ppl) == "ppl");
constexpr category empty_category{};
static_assert(empty_category.begin() == nullptr && empty_category.end() == nullptr);
static_assert(find(empty_category, "missing") == nullptr);
static_assert(symbol_count(operation{}) == 0);
static_assert(symbol_count(operation{"", " \t ", ""}) == 0);
static_assert(!references_symbol(operation{}, ""));
static_assert(!metanum::catalog::detail::symbols_well_formed(std::array<operation, 1>{{operation{}}}));
static_assert(!metanum::catalog::detail::parens_balanced(")("));
static_assert(!metanum::catalog::detail::parens_balanced("f((x)"));
static_assert(symbol_count(operation{"", "f(x, g(y/z)), h / j", ""}) == 3);
static_assert(!metanum::catalog::detail::category_well_formed(
    category{"gsl.bad", static_cast<library>(255), "Description", nullptr, 0}));
static_assert(!metanum::catalog::detail::category_well_formed(
    category{"gsl.bad", library::gsl, "Description", nullptr, 1}));
static_assert(!metanum::catalog::detail::category_well_formed(
    category{"gsl.bad.id", library::gsl, "Description", nullptr, 0}));
#ifdef METANUM_CPLUSPLUS
#error "catalog_core.hpp leaked its language-version macro"
#endif

// ---- runtime -----------------------------------------------------------------
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)
template <class E, class F> bool throws(F f) { try { (void)f(); } catch (const E&) { return true; } catch (...) {} return false; }
static bool near(double a, double b, double tol = 1e-12) { return std::fabs(a - b) <= tol * (1 + std::fabs(b)); }

int other_tu();  // defined in test_other.cpp (ODR / multi-TU check)

int main() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const double big = std::numeric_limits<double>::max();

    std::vector<double> v{2, 4, 4, 4, 5, 5, 7, 9};
    CHECK(mean(v) == 5);
    CHECK(near(variance(v, false), 4));
    CHECK(near(standard_deviation(v, false), 2));
    CHECK(near(variance(v), 32.0 / 7));
    CHECK(median(v) == 4.5);
    CHECK(quantile(v, 0) == 2 && quantile(v, 1) == 9);
    CHECK(near(quantile(v, 0.25), 4));  // R type 7
    CHECK((min)(v) == 2 && (max)(v) == 9);

    // shape errors
    CHECK(throws<std::invalid_argument>([] { return mean(nullptr, 3); }));
    CHECK(throws<std::invalid_argument>([] { return mean(std::vector<double>{}); }));
    CHECK(throws<std::invalid_argument>([] { return variance(std::vector<double>{1}); }));
    CHECK(variance(std::vector<double>{1}, false) == 0);
    CHECK(throws<std::invalid_argument>([&] { return covariance(v, std::vector<double>{1, 2}); }));
    CHECK(throws<std::invalid_argument>([&] { return covariance(v.data(), nullptr, v.size()); }));
    CHECK(throws<std::invalid_argument>([&] { return quantile(v, 1.5); }));
    CHECK(throws<std::invalid_argument>([&] { return quantile(v, nan); }));
    CHECK(throws<std::domain_error>([] { return correlation(std::vector<double>{1, 1, 1}, std::vector<double>{1, 2, 3}); }));

    // NaN: propagate, never UB in sorting
    std::vector<double> withnan{3, nan, 1, 2, nan, 0};
    CHECK(std::isnan(median(withnan)));
    CHECK(std::isnan(quantile(withnan, 0.3)));
    CHECK(std::isnan((min)(withnan)) && std::isnan((max)(withnan)));
    CHECK(std::isnan(mean(withnan)) && std::isnan(variance(withnan)));
    CHECK(std::isnan(correlation(withnan, std::vector<double>{1, 2, 3, 4, 5, 6})));

    // infinities
    CHECK(mean(std::vector<double>{1, inf, 2}) == inf);
    CHECK(std::isnan(mean(std::vector<double>{-inf, inf})));
    CHECK(quantile(std::vector<double>{inf, inf, inf}, 0.5) == inf);   // old code: inf*0 -> NaN
    CHECK(quantile(std::vector<double>{-inf, 0, 1}, 0.25) == -inf);
    CHECK(median(std::vector<double>{inf, inf}) == inf);

    // overflow robustness
    CHECK(mean(std::vector<double>{big, big}) == big);
    CHECK(median(std::vector<double>{big, big}) == big);
    CHECK(near(quantile(std::vector<double>{-big, big}, 0.75), big / 2));
    CHECK(near(correlation(std::vector<double>{1e200, -1e200, 3e200}, std::vector<double>{1, -1, 3}), 1));
    CHECK(standard_deviation(std::vector<double>{-big, big}, false) == big);
    CHECK(near(standard_deviation(std::vector<double>{-1e200, 1e200}, false) / 1e200, 1));
    CHECK(near(standard_deviation(std::vector<double>{-1e-200, 1e-200}, false) / 1e-200, 1));
    const double tiny = std::numeric_limits<double>::denorm_min();
    CHECK(standard_deviation(std::vector<double>{-tiny, tiny}, false) == tiny);
    CHECK(near(correlation(std::vector<double>{-tiny, tiny}, std::vector<double>{-big, big}), 1));
    CHECK(near(covariance(std::vector<double>{-1e200, 1e200},
                          std::vector<double>{-1e-200, 1e-200}, false), 1));
    CHECK(std::isnan(standard_deviation(std::vector<double>{1, inf})));
    CHECK(std::isnan(correlation(std::vector<double>{nan, 1}, std::vector<double>{2, 2})));

    // compensated summation / two-pass accuracy
    std::vector<double> c{1e16, 1, -1e16};
    CHECK(near(mean(c), 1.0 / 3));
    std::vector<double> off{1e9 + 4, 1e9 + 7, 1e9 + 13, 1e9 + 16};
    CHECK(near(variance(off), 30));

    // correlation bounds and sign
    std::vector<double> a{1, 2, 3, 4, 5}, b{10, 8, 6, 4, 2};
    CHECK(correlation(a, b) == -1);
    CHECK(correlation(a, a) == 1);

    // inputs unmodified
    std::vector<double> u{5, 1, 4, 2, 3};
    const auto copy = u;
    (void)median(u.data(), u.size());
    (void)quantile(u.data(), u.size(), 0.4);
    CHECK(u == copy);

    CHECK(other_tu() == 298);
    std::printf(failures ? "%d FAILURES\n" : "all tests passed\n", failures);
    return failures != 0;
}
