#include "fio.h"
#include "util.h"

#define READ_BUFFER_INC 1024

void *FIO_ReadFile(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    ASSERT_FMT(f != NULL, "Failed to open %s", path);

    size_t buf_len = 0;
    size_t buf_size = READ_BUFFER_INC;

    char *buf = ALLOC_N(buf_size, char);
    ASSERT_FMT(buf != NULL, "Failed to alloc %zu bytes", buf_size);

    size_t req, got;
    do {
        if (buf_len >= buf_size) {
            buf_size += READ_BUFFER_INC;
            buf = REALLOC(buf, buf_size, char);
            ASSERT_FMT(buf != NULL, "Failed to alloc %zu bytes", buf_size);
        }

        req = buf_size - buf_len;
        got = fread(buf + buf_len, 1, req, f);
        buf_len += got;
    } while (req == got);

    // returns non-zero if EOF is set
    ASSERT_FMT(feof(f) != 0, "EOF not reached %s", path);
    // returns zero if closed successfully
    ASSERT_FMT(fclose(f) == 0, "Failed to close %s", path);

    *size = buf_len;
    return buf;
}
