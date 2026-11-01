#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MULIB_IMPL_HASH
#define MULIB_IMPL_HATTRIE
#include "mulib-hattrie.h"
static int visit(const void *key, size_t length, uint64_t value, void *ctx) {
    size_t *count = (size_t *)ctx;
    assert(length >= 5 && !memcmp(key, "fruit", 5)); assert(value < 100);
    ++*count; return 1;
}
int main(void) {
    mulib_hattrie_t trie; uint64_t value; size_t count = 0;
    mulib_hattrie_init(&trie);
    for (unsigned int i = 0; i < 100; ++i) {
        char key[32]; int length = snprintf(key, sizeof(key), "fruit/%u", i);
        assert(length > 0 && mulib_hattrie_set(&trie, key, (size_t)length, i));
    }
    assert(mulib_hattrie_get(&trie, "fruit/42", 8, &value) && value == 42);
    assert(mulib_hattrie_prefix(&trie, "fruit", 5, visit, &count) == 100 && count == 100);
    assert(mulib_hattrie_remove(&trie, "fruit/42", 8, NULL));
    assert(mulib_hattrie_validate(&trie)); mulib_hattrie_free(&trie); return 0;
}
