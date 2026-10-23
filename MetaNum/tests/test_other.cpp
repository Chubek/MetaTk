// Second TU including everything: catches ODR / multiple-definition errors at link.
// Also simulates <windows.h> min/max macros being defined before inclusion.
// Standard headers come first, as they would on Windows (where the STL
// itself is macro-safe but libstdc++ is not).
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#include "all.hpp"
int other_tu() {
    const double d[] = {3, 1, 2};
    if ((metanum::min)(d, 3) != 1 || (metanum::max)(d, 3) != 3) return -1;
    return static_cast<int>(metanum::catalog::total_operations);
}
