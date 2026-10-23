#include "metalib/c/MetaML.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "C API failure, line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static metaml_status eval(metaml_c_session *s, const char *src, metaml_text *out) {
    return metaml_c_eval(s, src, strlen(src), out);
}
int main(int argc, char **argv) {
    metaml_c_session *s = NULL;
    metaml_text out = {0};
    CHECK(argc == 2);
    CHECK(metaml_session_create(NULL) == METAML_INVALID_ARGUMENT);
    CHECK(metaml_session_create(&s) == METAML_OK);
    CHECK(metaml_c_eval(s, NULL, 1, &out) == METAML_INVALID_ARGUMENT);
    CHECK(!out.data && !out.size);
    CHECK(metaml_install_metalib(s) == METAML_OK);
    CHECK(metaml_install_metalib(s) == METAML_OK);
    CHECK(metaml_c_load_stdlib(s, argv[1]) == METAML_OK);
    CHECK(eval(s, "List.sum [1, 2, 3]", &out) == METAML_OK);
    CHECK(out.size == 1 && !strcmp(out.data, "6"));
    metaml_text_free(&out);
    CHECK(eval(s, "Hash.sha256 \"abc\"", &out) == METAML_LANGUAGE_ERROR);
    CHECK(strstr(metaml_last_error(s), "M0403"));
    { const char *allow[] = {"Hash.*", "Buffer.*"};
      CHECK(metaml_allow_primitives(s, allow, 2) == METAML_OK); }
    CHECK(eval(s, "Hash.hex 255", &out) == METAML_OK);
    CHECK(!strcmp(out.data, "\"00000000000000ff\""));
    metaml_text_free(&out);
    CHECK(eval(s, "1 / 0", &out) == METAML_LANGUAGE_ERROR);
    CHECK(strstr(metaml_last_error(s), "M0402"));
    CHECK(eval(s, "print \"a\\0b\"", &out) == METAML_OK);
    metaml_text_free(&out);
    CHECK(metaml_output(s, &out, 1) == METAML_OK);
    CHECK(out.size == 4 && !memcmp(out.data, "a\0b\n", 4));
    metaml_text_free(&out);
    CHECK(metaml_c_type_of(s, "fn x -> x", 9, &out) == METAML_OK);
    CHECK(strstr(out.data, "->"));
    metaml_text_free(&out);
    CHECK(eval(s, "40 + 2", &out) == METAML_OK);
    CHECK(!strcmp(out.data, "42"));
    metaml_text_free(&out);
    metaml_text_free(&out);
    metaml_session_destroy(s);
    metaml_session_destroy(NULL);
    CHECK(eval(NULL, "1", &out) == METAML_INVALID_ARGUMENT);
    return 0;
}
