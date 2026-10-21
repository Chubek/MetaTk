#if defined(_WIN32)
#  if defined(TDYNLOAD_FIXTURE_BUILD)
#    define TDYNLOAD_FIXTURE_API __declspec(dllexport)
#  else
#    define TDYNLOAD_FIXTURE_API __declspec(dllimport)
#  endif
#else
#  define TDYNLOAD_FIXTURE_API __attribute__((visibility("default")))
#endif

TDYNLOAD_FIXTURE_API int tdynload_fixture_add(int a, int b) {
  return a + b;
}

TDYNLOAD_FIXTURE_API int tdynload_fixture_value = 42;

TDYNLOAD_FIXTURE_API const char* tdynload_fixture_name(void) {
  return "tdynload-fixture";
}
