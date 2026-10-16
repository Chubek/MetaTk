#include <stdint.h>
#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
EXPORT int64_t fixture_add(int64_t a,int64_t b) { return a+b; }
EXPORT int8_t fixture_i8(int8_t v) { return v; }
EXPORT uint8_t fixture_u8(uint8_t v) { return v; }
EXPORT int16_t fixture_i16(int16_t v) { return v; }
EXPORT uint16_t fixture_u16(uint16_t v) { return v; }
EXPORT int32_t fixture_i32(int32_t v) { return v; }
EXPORT uint32_t fixture_u32(uint32_t v) { return v; }
EXPORT uint64_t fixture_u64(uint64_t v) { return v; }
EXPORT double fixture_mixed(int8_t a,uint16_t b,float c,double d) { return a+b+c+d; }
EXPORT float fixture_float(float v) { return v*2; }
EXPORT void *fixture_pointer(void *p) { return p; }
EXPORT void fixture_write(int32_t *p,int32_t v) { *p=v; }
EXPORT int32_t fixture_zero(void) { return 42; }
