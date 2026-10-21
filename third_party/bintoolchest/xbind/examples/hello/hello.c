#include "hello.h"

int hello_add(int left, int right) { return left + right; }

double hello_scale(double value, double factor) { return value * factor; }

const char *hello_greet(const char *name) { return name ? name : "world"; }
