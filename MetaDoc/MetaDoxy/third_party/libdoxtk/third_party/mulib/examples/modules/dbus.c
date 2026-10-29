#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_DBUS
#include "mulib-dbus.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    mulib_dbus_value_t value = {0};
    mulib_dbus_message_t call = {0}, decoded = {0};
    unsigned char *wire = NULL;
    size_t length, used;
    value.type = 's'; value.text = "hello"; value.length = 5;
    call.endian = 'l'; call.type = 1; call.serial = 1;
    call.path = "/org/example/Object"; call.interface = "org.example.Service";
    call.member = "Echo"; call.body = &value; call.count = 1;
    assert(mulib_dbus_encode(&call, NULL, &wire, &length) == MULIB_DBUS_OK);
    assert(wire[0] == 'l' && wire[1] == 1 && wire[3] == 1);
    assert(mulib_dbus_decode(wire, length-1, NULL, &decoded, &used) == MULIB_DBUS_MORE);
    assert(mulib_dbus_decode(wire, length, NULL, &decoded, &used) == MULIB_DBUS_OK);
    assert(used == length && !strcmp(decoded.signature, "s"));
    assert(decoded.count == 1 && !strcmp(decoded.body[0].text, "hello"));
    mulib_dbus_message_free(&decoded); free(wire);
    return 0;
}
