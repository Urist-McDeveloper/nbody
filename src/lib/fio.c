#include "fio.h"
#include "util.h"

#define READ_BUFFER_INC 1024

void *FIO_ReadFile(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    ASSERT(f != NULL);

    size_t buf_len = 0;
    size_t buf_size = READ_BUFFER_INC;
    unsigned char *buf = ALLOC_N(buf_size,unsigned char);

    size_t req, got;
    do {
        if (buf_len >= buf_size) {
            buf_size += READ_BUFFER_INC;
            buf = REALLOC(buf, buf_size, unsigned char);
            ASSERT(buf != NULL);
        }

        req = buf_size - buf_len;
        got = fread(buf + buf_len, 1, req, f);
        buf_len += got;
    } while (req == got);

    ASSERT(feof(f) != 0);   // returns non-zero if EOF is set
    ASSERT(fclose(f) == 0); // returns zero if closed successfully

    *size = buf_len;
    return buf;
}
