#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_ELF
#include "mulib-elf.h"
#include <assert.h>

int main(void) {
    /* A minimal little-endian ELF64 relocatable header, with no tables. */
    unsigned char bytes[64] = { 0x7f, 'E', 'L', 'F', 2, 1, 1 };
    mulib_elf_t view;
    mulib_elf_section_t section;
    bytes[16] = 1; bytes[18] = 62; bytes[20] = 1; bytes[52] = 64;
    assert(mulib_elf_open(&view, bytes, sizeof(bytes)) == 0);
    assert(view.bits == 64 && view.machine == 62 && view.shnum == 0);
    assert(mulib_elf_section(&view, 0, &section) == MULIB_ELF_EBOUNDS);
    bytes[5] = 0;
    assert(mulib_elf_open(&view, bytes, sizeof(bytes)) == MULIB_ELF_EFORMAT);
    return 0;
}
