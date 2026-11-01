#define MULIB_IMPL_MAP
#define MULIB_IMPL_VEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_MINISH
#include "mulib-minish.h"
#include <assert.h>
#include <string.h>

static int capture(void *user, const char *text, size_t length) {
    return mulib_strbuf_append_n((mulib_strbuf_t *)user, text, length);
}
static int add(mulib_minish_t *shell, size_t argc, const char *const *argv, void *user) {
    (void)user;
    if (argc != 3 || strcmp(argv[1], "20") || strcmp(argv[2], "22")) return 2;
    return mulib_minish_set(shell, "answer", "42") == MULIB_MINISH_OK ? 0 : 1;
}
int main(void) {
    const char script[] = "set who 'MuLIB user'; echo \"Hello, ${who}\"; add 20 22; echo $answer";
    mulib_minish_t shell;
    mulib_strbuf_t output;
    mulib_minish_error_t error;
    mulib_minish_init(&shell);
    mulib_strbuf_init(&output);
    mulib_minish_set_writer(&shell, capture, &output);
    assert(mulib_minish_register(&shell, "add", add, NULL) == MULIB_MINISH_OK);
    assert(mulib_minish_execute(&shell, script, sizeof(script) - 1, &error) == MULIB_MINISH_OK);
    assert(!strcmp(output.data, "Hello, MuLIB user\n42\n"));
    assert(shell.last_status == 0 && !shell.allow_process);
    mulib_minish_free(&shell);
    mulib_strbuf_free(&output);
    return 0;
}
