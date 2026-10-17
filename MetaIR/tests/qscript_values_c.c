#include "qscript/include/QScript-Runtime.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "typed C API line %d: %s (%s)\n", __LINE__, #x, qs_error(c)); goto fail; } } while (0)
int qscript_values_c(void) {
    qs_context *c = qs_create();
    qs_program *p = NULL;
    qs_value *values[12] = {NULL};
    const qs_value *args[2];
    const char *text;
    size_t length, i;
    int64_t integer;
    const char *source =
        "fn echo(s: str): str { return s ~ \"!\"; }"
        "fn push(xs: list<int>): list<int> { xs.push(9); return xs; }"
        "fn parse(g: grammar, s: str): match { return g.parse(s); }"
        "fn number(n: int): int { return n+1; }";
    CHECK(c);
    p = qs_compile(c, source, strlen(source)); CHECK(p);
    values[0] = qs_value_string(c, "a\0b", 3); CHECK(values[0]);
    args[0] = values[0];
    CHECK(qs_call_value(c, p, "echo", args, 1, &values[1]) == QS_OK);
    CHECK(qs_value_get_string(values[1], &text, &length) == QS_OK && length == 4 && !memcmp(text, "a\0b!", 4));
    CHECK(qs_value_get_kind(values[1]) == QS_VALUE_STR && !strcmp(qs_value_type(values[1]), "str"));
    CHECK(qs_value_get_int(values[1], &integer) == QS_INVALID_ARGUMENT && integer == 0);
    values[2] = qs_value_int(c, 41); CHECK(values[2]);
    args[0] = values[2];
    CHECK(qs_call_value(c, p, "number", args, 1, &values[3]) == QS_OK);
    CHECK(qs_value_get_int(values[3], &integer) == QS_OK && integer == 42);
    values[4] = qs_value_list(c, "int", args, 1); CHECK(values[4]);
    args[0] = values[4];
    CHECK(qs_call_value(c, p, "push", args, 1, &values[5]) == QS_OK);
    CHECK(qs_value_list_size(values[4]) == 1 && qs_value_list_size(values[5]) == 2);
    values[6] = qs_value_list_at(c, values[5], 1); CHECK(values[6]);
    CHECK(qs_value_get_int(values[6], &integer) == QS_OK && integer == 9);
    CHECK(!qs_value_list_at(c, values[5], 99));
    values[7] = qs_value_grammar(c, "grammar G { token TOP { 'a' } }", strlen("grammar G { token TOP { 'a' } }")); CHECK(values[7]);
    CHECK(qs_value_get_grammar(values[7]) && !strcmp(qs_grammar_name(qs_value_get_grammar(values[7])), "G"));
    values[8] = qs_value_string(c, "a", 1); CHECK(values[8]);
    args[0] = values[7]; args[1] = values[8];
    CHECK(qs_call_value(c, p, "parse", args, 2, &values[9]) == QS_OK);
    CHECK(qs_value_get_match(values[9]) && !strcmp(qs_match_name(qs_value_get_match(values[9])), "TOP"));
    values[10] = qs_value_match(c, qs_value_get_match(values[9])); CHECK(values[10]);
    values[11] = qs_value_clone(c, values[10]); CHECK(values[11]);
    for (i = 0; i < qs_program_function_count(p); ++i) {
        CHECK(qs_program_function_result_type(p, i));
        CHECK(qs_program_function_parameter_type(p, i, 0));
        CHECK(!qs_program_function_parameter_type(p, i, 100));
    }
    CHECK(!qs_program_function_result_type(p, 100));
    args[0] = values[2];
    { qs_value *bad = NULL;
      CHECK(qs_call_value(c, p, "echo", args, 1, &bad) == QS_ERROR && !bad && strstr(qs_error(c), "expects str, got int"));
      CHECK(qs_call_value(c, p, "echo", NULL, 1, &bad) == QS_INVALID_ARGUMENT && !bad);
      args[0] = NULL;
      CHECK(qs_call_value(c, p, "echo", args, 1, &bad) == QS_INVALID_ARGUMENT && !bad);
      CHECK(qs_call_value(c, p, "echo", args, 17, &bad) == QS_INVALID_ARGUMENT && !bad);
    }
    CHECK(!qs_value_string(c, NULL, 1));
    CHECK(!qs_value_list(c, "list<bad>", NULL, 0));
    CHECK(qs_value_get_kind(NULL) == -1 && !qs_value_type(NULL));
    qs_program_destroy(p); p = NULL;
    qs_destroy(c); c = NULL;
    CHECK(qs_value_get_string(values[1], &text, &length) == QS_OK && length == 4);
    CHECK(qs_value_get_match(values[11]) && !strcmp(qs_match_text(qs_value_get_match(values[11]), &length), "a"));
    for (i = 0; i < 12; ++i) qs_value_destroy(values[i]);
    return 1;
fail:
    for (i = 0; i < 12; ++i) qs_value_destroy(values[i]);
    qs_program_destroy(p); qs_destroy(c); return 0;
}
