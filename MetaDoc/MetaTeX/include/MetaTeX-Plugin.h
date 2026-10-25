#pragma once
#include "MetaTeX.h"
// Every dynamically loaded plugin exposes these two unmangled entry points.
// The registration function is C++ ABI-specific and must be built with the
// same standard-library ABI and MetaTeX shared library as the host.
extern "C" const char* metatex_plugin_id();
extern "C" void metatex_register_plugin(metatex::Engine* engine);
