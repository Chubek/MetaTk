#define MULIB_IMPL_MMFILE
#include "mulib-mmfile.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
#if defined(__unix__) || defined(__APPLE__)
    const char *path = "mulib-mmfile-example.tmp";
    mulib_mmfile_t mapping = MULIB_MMFILE_INIT;
    FILE *file = fopen(path, "wbx");
    assert(file);
    assert(fwrite("hello", 1, 5, file) == 5);
    assert(fclose(file) == 0);
    assert(mulib_mmfile_open(&mapping, path, 1) == 0 && mapping.size == 5);
    ((unsigned char *)mapping.data)[0] = 'H';
    assert(mulib_mmfile_flush(&mapping) == 0);
    assert(mulib_mmfile_close(&mapping) == 0);
    file = fopen(path, "rb");
    assert(file && fgetc(file) == 'H');
    assert(fclose(file) == 0 && remove(path) == 0);
#else
    mulib_mmfile_t mapping = MULIB_MMFILE_INIT;
    assert(mulib_mmfile_open(&mapping, "unused", 0) == MULIB_MMFILE_ENOTSUP);
#endif
    return 0;
}
