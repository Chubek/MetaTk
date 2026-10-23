// MetaNum - portable, header-only descriptive statistics.
//
// Contract (applies to every function below):
//   * Invalid shape -> std::invalid_argument: null pointer with n > 0, empty
//     input, too few points for a sample estimate, mismatched lengths, or a
//     probability outside [0, 1].
//   * NaN in the data -> the result is NaN. No function throws for NaN data,
//     and NaN never reaches std::sort / std::nth_element (where it would be
//     undefined behaviour).
//   * +/-inf in the data propagates according to IEEE arithmetic.
//   * correlation() of constant data -> std::domain_error.
//   * Inputs are never modified; by-value vector parameters are scratch copies.
//
// Accuracy: sums use compensated (Neumaier) summation in long double, falling
// back to a scaled sum if the plain sum would overflow; variance/covariance
// use power-of-two scaling and a compensated, corrected two-pass algorithm.
// quantile() uses linear interpolation
// between order statistics (Hyndman-Fan type 7; the same convention as
// gsl_stats_quantile_from_sorted_data and R's default).
#ifndef METANUM_NUMERIC_HPP
#define METANUM_NUMERIC_HPP

#if defined(_MSVC_LANG)
#  define METANUM_NUMERIC_CPLUSPLUS _MSVC_LANG
#else
#  define METANUM_NUMERIC_CPLUSPLUS __cplusplus
#endif
#if METANUM_NUMERIC_CPLUSPLUS < 201703L
#  error "MetaNum requires C++17 or later."
#endif
#undef METANUM_NUMERIC_CPLUSPLUS

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

// Note: min/max are declared as (min)/(max) so that the function-like macros
// defined by <windows.h> (without NOMINMAX) cannot expand them. Call them as
// (metanum::min)(...) and (metanum::max)(...) in such environments.

namespace metanum {
namespace detail {

inline constexpr double nan_v = std::numeric_limits<double>::quiet_NaN();

inline void require(bool ok, const char* msg) {
    if (!ok) throw std::invalid_argument(msg);
}

inline void require_data(const double* x, std::size_t n, std::size_t min_n, const char* msg) {
    require(n >= min_n && n > 0 && x != nullptr, msg);
}

inline bool has_nan(const double* x, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i)
        if (std::isnan(x[i])) return true;
    return false;
}

/// Neumaier-compensated sum of v[i] * scale. Non-finite terms disable the
/// compensation (which would otherwise turn inf into NaN).
inline long double compensated_sum(const double* x, std::size_t n, long double scale) noexcept {
    long double s = 0.0L, c = 0.0L;
    bool finite = true;
    for (std::size_t i = 0; i < n; ++i) {
        const long double v = static_cast<long double>(x[i]) * scale;
        const long double t = s + v;
        if (finite && std::isfinite(t)) {
            if (std::fabs(s) >= std::fabs(v)) c += (s - t) + v;
            else                              c += (v - t) + s;
        } else {
            finite = false;
        }
        s = t;
    }
    return finite ? s + c : s;
}

/// Mean in extended precision. Falls back to summing x[i]/n when the direct
/// sum overflows although every input is finite.
inline long double mean_ld(const double* x, std::size_t n) noexcept {
    const long double ln = static_cast<long double>(n);
    const long double s = compensated_sum(x, n, 1.0L);
    if (std::isfinite(s)) return s / ln;
    for (std::size_t i = 0; i < n; ++i)
        if (!std::isfinite(x[i])) return s / ln;  // genuine inf/NaN: propagate
    return compensated_sum(x, n, 1.0L / ln);
}

/// Used only for bounded, finite normalized terms.
struct compensated_accumulator {
    long double sum = 0.0L;
    long double correction = 0.0L;

    void add(long double v) noexcept {
        const long double t = sum + v;
        correction += std::fabs(sum) >= std::fabs(v) ? (sum - t) + v : (v - t) + sum;
        sum = t;
    }
    [[nodiscard]] long double value() const noexcept { return sum + correction; }
};

struct normalized_series {
    int exponent = 0;
    long double mean = 0.0L;
};

// Scaling by an exact power of two avoids overflowing differences/products,
// including on platforms where long double has the same range as double.
inline normalized_series normalize(const double* x, std::size_t n) noexcept {
    double largest = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isfinite(x[i])) return {0, nan_v};
        largest = (std::max)(largest, std::fabs(x[i]));
    }
    int exponent = 0;
    (void)std::frexp(largest, &exponent);
    compensated_accumulator sum;
    for (std::size_t i = 0; i < n; ++i)
        sum.add(std::ldexp(static_cast<long double>(x[i]), -exponent));
    return {exponent, sum.value() / static_cast<long double>(n)};
}

/// Corrected two-pass cross product in normalized coordinates.
inline long double centered_cross(const double* x, normalized_series nx,
                                  const double* y, normalized_series ny, std::size_t n) noexcept {
    if (std::isnan(nx.mean) || std::isnan(ny.mean)) return nan_v;
    compensated_accumulator sxy, sx, sy;
    for (std::size_t i = 0; i < n; ++i) {
        const long double dx = std::ldexp(static_cast<long double>(x[i]), -nx.exponent) - nx.mean;
        const long double dy = std::ldexp(static_cast<long double>(y[i]), -ny.exponent) - ny.mean;
        sxy.add(dx * dy);
        sx.add(dx);
        sy.add(dy);
    }
    return sxy.value() - (sx.value() / static_cast<long double>(n)) * sy.value();
}

inline double to_double_nonneg(long double v) noexcept {
    // Rounding can make a mathematically non-negative quantity slightly negative.
    if (v < 0.0L) v = 0.0L;
    return static_cast<double>(v);
}

/// Overflow-safe midpoint of two doubles.
inline double midpoint(double a, double b) noexcept {
    constexpr double hi = (std::numeric_limits<double>::max)() / 2;
    if (std::fabs(a) <= hi && std::fabs(b) <= hi) return (a + b) / 2;
    return a / 2 + b / 2;
}

/// Interpolate between order statistics a <= b with weight f in [0, 1]:
/// exact at the endpoints, monotone, overflow-safe, and inf-aware.
inline double lerp_sorted(double a, double b, double f) noexcept {
    if (f <= 0.0 || a == b) return a;
    if (f >= 1.0) return b;
    if (std::isinf(a) || std::isinf(b) || (a < 0.0) != (b < 0.0))
        return a * (1.0 - f) + b * f;         // b - a may overflow here
    const double r = a + f * (b - a);         // same sign: b - a is exact-range
    return std::clamp(r, a, b);
}

inline double median_inplace(std::vector<double>& x) {
    require(!x.empty(), "metanum::median: requires non-empty data");
    if (has_nan(x.data(), x.size())) return nan_v;
    const auto mid = x.begin() + static_cast<std::ptrdiff_t>(x.size() / 2);
    std::nth_element(x.begin(), mid, x.end());
    const double hi = *mid;
    if (x.size() % 2 != 0) return hi;
    const double lo = *std::max_element(x.begin(), mid);
    return midpoint(lo, hi);
}

inline double quantile_inplace(std::vector<double>& x, double p) {
    require(!x.empty(), "metanum::quantile: requires non-empty data");
    require(p >= 0.0 && p <= 1.0, "metanum::quantile: p must be in [0, 1]");  // also rejects NaN
    if (has_nan(x.data(), x.size())) return nan_v;
    const std::size_t last = x.size() - 1;
    const long double pos = static_cast<long double>(p) * static_cast<long double>(last);
    std::size_t lo = static_cast<std::size_t>(pos);
    if (lo > last) lo = last;
    const double f = static_cast<double>(pos - static_cast<long double>(lo));
    const auto it = x.begin() + static_cast<std::ptrdiff_t>(lo);
    std::nth_element(x.begin(), it, x.end());
    const double a = *it;
    if (lo == last || f == 0.0) return a;
    const double b = *std::min_element(it + 1, x.end());
    return lerp_sorted(a, b, f);
}

}  // namespace detail

// ---------------------------------------------------------------------------
// Pointer + length interface
// ---------------------------------------------------------------------------

/// Arithmetic mean.
[[nodiscard]] inline double mean(const double* x, std::size_t n) {
    detail::require_data(x, n, 1, "metanum::mean: requires non-empty data");
    return static_cast<double>(detail::mean_ld(x, n));
}

/// Variance; sample (n-1 denominator) by default, population (n) otherwise.
[[nodiscard]] inline double variance(const double* x, std::size_t n, bool sample = true) {
    detail::require_data(x, n, sample ? 2 : 1,
                         "metanum::variance: requires n >= 1 (population) or n >= 2 (sample)");
    const auto m = detail::normalize(x, n);
    const long double ss = detail::centered_cross(x, m, x, m, n);
    return detail::to_double_nonneg(std::ldexp(ss / static_cast<long double>(sample ? n - 1 : n),
                                             2 * m.exponent));
}

/// Standard deviation; sample by default.
[[nodiscard]] inline double standard_deviation(const double* x, std::size_t n, bool sample = true) {
    detail::require_data(x, n, sample ? 2 : 1,
                         "metanum::standard_deviation: requires n >= 1 (population) or n >= 2 (sample)");
    const auto m = detail::normalize(x, n);
    long double ss = detail::centered_cross(x, m, x, m, n);
    if (ss < 0.0L) ss = 0.0L;
    return static_cast<double>(std::ldexp(std::sqrt(ss / static_cast<long double>(sample ? n - 1 : n)),
                                          m.exponent));
}

/// Covariance of two equally long series; sample by default.
[[nodiscard]] inline double covariance(const double* x, const double* y, std::size_t n,
                                       bool sample = true) {
    detail::require_data(x, n, sample ? 2 : 1,
                         "metanum::covariance: requires n >= 1 (population) or n >= 2 (sample)");
    detail::require(y != nullptr, "metanum::covariance: y is null");
    const auto mx = detail::normalize(x, n), my = detail::normalize(y, n);
    const long double s = detail::centered_cross(x, mx, y, my, n);
    return static_cast<double>(std::ldexp(s / static_cast<long double>(sample ? n - 1 : n),
                                         mx.exponent + my.exponent));
}

/// Pearson correlation coefficient in [-1, 1].
/// Throws std::domain_error if either series is constant.
[[nodiscard]] inline double correlation(const double* x, const double* y, std::size_t n) {
    detail::require_data(x, n, 2, "metanum::correlation: requires n >= 2");
    detail::require(y != nullptr, "metanum::correlation: y is null");
    const auto mx = detail::normalize(x, n), my = detail::normalize(y, n);
    const long double sxx = detail::centered_cross(x, mx, x, mx, n);
    const long double syy = detail::centered_cross(y, my, y, my, n);
    const long double sxy = detail::centered_cross(x, mx, y, my, n);
    if (std::isnan(sxx) || std::isnan(syy) || std::isnan(sxy)) return detail::nan_v;
    if (sxx <= 0.0L || syy <= 0.0L)
        throw std::domain_error("metanum::correlation: undefined for constant data");
    // Divide separately to avoid forming a potentially extreme product.
    const long double r = (sxy / std::sqrt(sxx)) / std::sqrt(syy);
    if (std::isnan(r)) return detail::nan_v;
    return static_cast<double>(std::clamp(r, -1.0L, 1.0L));
}

/// Median (mean of the two middle values for even n). O(n).
[[nodiscard]] inline double median(const double* x, std::size_t n) {
    detail::require_data(x, n, 1, "metanum::median: requires non-empty data");
    std::vector<double> tmp(x, x + n);
    return detail::median_inplace(tmp);
}

/// p-quantile, p in [0, 1], type-7 linear interpolation. O(n).
[[nodiscard]] inline double quantile(const double* x, std::size_t n, double p) {
    detail::require_data(x, n, 1, "metanum::quantile: requires non-empty data");
    detail::require(p >= 0.0 && p <= 1.0, "metanum::quantile: p must be in [0, 1]");
    std::vector<double> tmp(x, x + n);
    return detail::quantile_inplace(tmp, p);
}

/// Smallest element; NaN if any element is NaN.
[[nodiscard]] inline double(min)(const double* x, std::size_t n) {
    detail::require_data(x, n, 1, "metanum::min: requires non-empty data");
    double m = x[0];
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(x[i])) return detail::nan_v;
        if (x[i] < m) m = x[i];
    }
    return m;
}

/// Largest element; NaN if any element is NaN.
[[nodiscard]] inline double(max)(const double* x, std::size_t n) {
    detail::require_data(x, n, 1, "metanum::max: requires non-empty data");
    double m = x[0];
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(x[i])) return detail::nan_v;
        if (x[i] > m) m = x[i];
    }
    return m;
}

// ---------------------------------------------------------------------------
// std::vector interface (length checked for paired series)
// ---------------------------------------------------------------------------

[[nodiscard]] inline double mean(const std::vector<double>& x) {
    return mean(x.data(), x.size());
}
[[nodiscard]] inline double variance(const std::vector<double>& x, bool sample = true) {
    return variance(x.data(), x.size(), sample);
}
[[nodiscard]] inline double standard_deviation(const std::vector<double>& x, bool sample = true) {
    return standard_deviation(x.data(), x.size(), sample);
}
[[nodiscard]] inline double covariance(const std::vector<double>& x, const std::vector<double>& y,
                                       bool sample = true) {
    detail::require(x.size() == y.size(), "metanum::covariance: x and y differ in length");
    return covariance(x.data(), y.data(), x.size(), sample);
}
[[nodiscard]] inline double correlation(const std::vector<double>& x, const std::vector<double>& y) {
    detail::require(x.size() == y.size(), "metanum::correlation: x and y differ in length");
    return correlation(x.data(), y.data(), x.size());
}
/// Takes a copy it may reorder; pass an rvalue to avoid the copy.
[[nodiscard]] inline double median(std::vector<double> x) {
    return detail::median_inplace(x);
}
/// Takes a copy it may reorder; pass an rvalue to avoid the copy.
[[nodiscard]] inline double quantile(std::vector<double> x, double p) {
    return detail::quantile_inplace(x, p);
}
[[nodiscard]] inline double(min)(const std::vector<double>& x) {
    return (min)(x.data(), x.size());
}
[[nodiscard]] inline double(max)(const std::vector<double>& x) {
    return (max)(x.data(), x.size());
}

}  // namespace metanum

#endif  // METANUM_NUMERIC_HPP
