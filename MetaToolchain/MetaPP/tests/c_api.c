#include "MetaPP.h"
#include <string.h>
int main(void) {
    metapp_context *ctx = metapp_create();
    char *output = NULL;
    if (!ctx || !metapp_define(ctx, "VALUE 42")) return 1;
    if (!metapp_preprocess(ctx, "VALUE", "test.c", &output)) return 2;
    if (strcmp(output, "42\n")) return 3;
    metapp_free(output);
    if (metapp_preprocess(ctx, "#error bad", "test.c", &output) || output) return 4;
    if (!strstr(metapp_diagnostics(ctx), "test.c:1: error: bad")) return 5;
    if (!metapp_undefine(ctx, "VALUE")) return 6;
    if (metapp_preprocess(ctx, NULL, "test.c", &output) || output) return 7;
    {
        const char bytes[] = {'4', '2', '\0', '#', 'e', 'r', 'r', 'o', 'r'};
        if (metapp_preprocess_n(ctx, bytes, sizeof bytes, "bytes.c", &output) || output) return 8;
        if (!strstr(metapp_diagnostics(ctx), "NUL byte")) return 9;
        if (!metapp_preprocess_n(ctx, bytes, 2, "bytes.c", &output)) return 10;
        if (strcmp(output, "42\n")) return 11;
        metapp_free(output);
        /* Reject oversize before reading from the supplied pointer. */
        if (metapp_preprocess_n(ctx, "", (size_t)-1, "bytes.c", &output) || output) return 12;
        if (!strstr(metapp_diagnostics(ctx), "input exceeds")) return 13;
        if (!metapp_preprocess_n(ctx, "", 0, "bytes.c", &output)) return 14;
        if (strcmp(output, "")) return 15;
        metapp_free(output);
        if (metapp_preprocess_n(NULL, "", 0, "bytes.c", &output) || output) return 16;
    }
    metapp_destroy(ctx);
    metapp_destroy(NULL);
    return 0;
}
