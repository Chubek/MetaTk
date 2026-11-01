#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_VARINT
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_KVDB
#include "mulib-kvdb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
#if defined(__unix__) || defined(__APPLE__)
    const char *path = "mulib-kvdb-example.tmp";
    const unsigned char key[] = { 'k', 0, 'y' }, value[] = { 0, 1, 255 };
    mulib_kvdb_t *db = NULL;
    const void *borrowed;
    size_t size;
    int result;
    assert(mulib_kvdb_open(path, 0, &db) == MULIB_KVDB_ENOTFOUND);
    assert(mulib_kvdb_open(path, 1, &db) == 0);
    assert(mulib_kvdb_set(db, key, sizeof(key), value, sizeof(value)) == 0);
    result = mulib_kvdb_commit(db);
    assert(result == 0 || result == MULIB_KVDB_UNCERTAIN);
    mulib_kvdb_close(db);
    assert(mulib_kvdb_open(path, 0, &db) == 0);
    assert(mulib_kvdb_get(db, key, sizeof(key), &borrowed, &size) == 0);
    assert(size == sizeof(value) && !memcmp(borrowed, value, size));
    mulib_kvdb_close(db);
    assert(remove(path) == 0);
#else
    assert(mulib_kvdb_commit(NULL) == MULIB_KVDB_EINVAL);
#endif
    return 0;
}
