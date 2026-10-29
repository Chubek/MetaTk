#pragma once
#include <cstdlib>
#include <iostream>

// Unlike assert(), checks remain active in Release builds.
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: " #expression "\n"; \
        std::exit(EXIT_FAILURE); \
    } \
} while (false)
