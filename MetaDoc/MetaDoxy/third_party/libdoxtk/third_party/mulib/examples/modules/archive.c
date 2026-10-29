#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ARCHIVE
#include "mulib-archive.h"
#include <assert.h>
#include <string.h>

int main(void) {
    mulib_archive_writer_t writer;
    mulib_archive_reader_t reader;
    mulib_archive_entry_t entry;
    const unsigned char value[] = { 'a', 0, 'b' };
    mulib_archive_writer_init(&writer);
    assert(mulib_archive_add_directory(&writer, "notes/", 0755, 0) == 0);
    assert(mulib_archive_add(&writer, "notes/data", value, sizeof(value), 0644, 0) == 0);
    assert(mulib_archive_add(&writer, "../escape", NULL, 0, 0644, 0) == MULIB_ARCHIVE_EUNSAFE);
    assert(mulib_archive_finish(&writer) == 0);
    assert(mulib_archive_reader_init(&reader, writer.bytes.data, writer.bytes.length) == 0);
    assert(mulib_archive_next(&reader, &entry) == 1 && entry.type == '5');
    assert(mulib_archive_next(&reader, &entry) == 1 && !strcmp(entry.path, "notes/data"));
    assert(entry.size == sizeof(value) && !memcmp(entry.data, value, sizeof(value)));
    assert(mulib_archive_next(&reader, &entry) == 0);
    mulib_archive_writer_free(&writer);
    return 0;
}
