#include <domocamlbind/capi.h>
CAMLprim value make_pair(value input) {
    CAMLparam1(input);
    CAMLlocal2(text, pair);
    text = ocamlconv_string("ok", 2);
    pair = ocamlblock_pair(input, text);
    CAMLreturn(pair);
}
