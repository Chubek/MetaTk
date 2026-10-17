#include <cstdint>

// ABI-only fixture: dispatch is supplied as an ASMtk object by the test, so
// its int return can deliberately leave the upper half of rax unspecified.
extern "C" std::int64_t qs_native_entry(void*);
int main() { return static_cast<unsigned char>(qs_native_entry(nullptr)); }
