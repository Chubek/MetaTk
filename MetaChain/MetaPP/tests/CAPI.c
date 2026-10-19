#include "MetaPP-C.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "C ABI check failed at line %d: %s\n", __LINE__, #expr); ++failures; } } while (0)

static const char* kernel(const char* args, const char* file, size_t line, void* data) {
    CHECK(strcmp(args, "9") == 0);
    CHECK(strcmp(file, "foreign.c") == 0);
    CHECK(line == 1);
    return (const char*)data;
}
static const char* bad_kernel(const char* args, const char* file, size_t line, void* data) {
    (void)args; (void)file; (void)line; (void)data;
    return NULL;
}

int main(void) {
    CHECK(metapp_abi_version() == METAPP_ABI_VERSION);
    CHECK(metapp_create(NULL, 1) == NULL);
    CHECK(metapp_process(NULL, "", NULL) == 1);
    CHECK(metapp_last_error(NULL)[0] != '\0');
    metapp_destroy(NULL);
    metapp_context* pp = metapp_create(NULL, 0);
    CHECK(pp != NULL);
    if (!pp) return 1;
    CHECK(metapp_define(pp, "X=9") == 0);
    CHECK(metapp_process(pp, "X", NULL) == 0);
    CHECK(strcmp(metapp_output(pp), "9\n") == 0);
    CHECK(metapp_register_kernel(pp, "foreign", kernel, (void*)"answer") == 0);
    CHECK(metapp_process(pp, "#kernel foreign X", "foreign.c") == 0);
    CHECK(strcmp(metapp_output(pp), "answer\n") == 0);
    CHECK(metapp_process(pp, "#warning hi", NULL) == 0);
    CHECK(metapp_warning_count(pp) == 1);
    CHECK(metapp_process(pp, "#error failure", NULL) == 1);
    CHECK(strstr(metapp_last_error(pp), "failure") != NULL);
    CHECK(metapp_output(pp)[0] == '\0');
    CHECK(metapp_define(pp, NULL) == 1);
    CHECK(metapp_undefine(pp, "X") == 0);
    CHECK(metapp_last_error(pp)[0] == '\0');
    CHECK(metapp_process(pp, "X", NULL) == 0);
    CHECK(strcmp(metapp_output(pp), "X\n") == 0);
    CHECK(metapp_register_kernel(pp, "bad", bad_kernel, NULL) == 0);
    CHECK(metapp_process(pp, "#kernel bad", NULL) == 1);
    CHECK(strstr(metapp_last_error(pp), "callback failed") != NULL);
    metapp_destroy(pp);
    return failures ? 1 : 0;
}
