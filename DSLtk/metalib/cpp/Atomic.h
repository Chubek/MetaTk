/** Synchronized signed 64-bit cells. Each operation is linearizable.
 * fetch_add wraps modulo 2^64 and returns the previous value.
 * compare_exchange returns (success, previous value). */
#pragma once
#include "Support.h"
#include <bit>
#include "Atomic.c"
