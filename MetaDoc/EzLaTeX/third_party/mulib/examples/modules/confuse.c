#define MULIB_IMPL_CONFUSE
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-confuse.h"
#include <assert.h>
#include <string.h>
int main(void) {
    const char *source = "port=8080; server main { enabled=yes }";
    mulib_parser_error_t error;
    mulib_confuse_t *config = mulib_confuse_parse(source, strlen(source), &error);
    const mulib_confuse_t *server;
    assert(config && mulib_confuse_getint(config, "port", 0) == 8080);
    server = mulib_confuse_get(config, "server", 0);
    assert(server && mulib_confuse_getbool(server, "enabled", 0));
    mulib_confuse_destroy(config); return 0;
}
