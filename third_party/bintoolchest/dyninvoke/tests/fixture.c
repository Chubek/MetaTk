// dyninvoke test fixture (shared library).
#if defined(_WIN32)
#if defined(DYNINVOKE_FIXTURE_BUILD)
#define FIX_API __declspec(dllexport)
#else
#define FIX_API __declspec(dllimport)
#endif
#else
#define FIX_API __attribute__((visibility("default")))
#endif

FIX_API int dinvk_add2(int a, int b) { return a + b; }
FIX_API long long dinvk_add64(long long a, long long b) { return a + b; }
FIX_API float dinvk_addf(float a, float b) { return a + b; }
FIX_API double dinvk_addd(double a, double b) { return a + b; }
FIX_API int dinvk_mixed(int a, double b, int c) { return a + (int)b + c; }
FIX_API int dinvk_sum8(int a1, int a2, int a3, int a4, int a5, int a6, int a7,
                       int a8) {
  return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8;
}
typedef struct {
  int x;
  int y;
} DinVKPair;
FIX_API DinVKPair dinvk_pair_add(DinVKPair a, DinVKPair b) {
  DinVKPair r;
  r.x = a.x + b.x;
  r.y = a.y + b.y;
  return r;
}
FIX_API int dinvk_call_cb(int (*cb)(int, int), int a, int b) {
  return cb(a, b);
}
FIX_API const char* dinvk_name(void) { return "dyninvoke-fixture"; }

typedef struct { long long integer; double real; } DinVKMixed;
FIX_API DinVKMixed dinvk_mixed_pair(long long a, double b) {
  DinVKMixed result = {a + 1, b * 2};
  return result;
}
typedef struct { double real; long long integer; } DinVKReverse;
FIX_API DinVKReverse dinvk_reverse_pair(double a, long long b) {
  DinVKReverse result = {a * 3, b + 2};
  return result;
}
typedef struct { long long x, y, z; } DinVKBig;
FIX_API DinVKBig dinvk_big(long long a) {
  DinVKBig result = {a, a + 1, a + 2};
  return result;
}
