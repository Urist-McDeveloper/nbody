#include "fio.h"
#include "util.h"

#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#define stat _stat
#endif

void *FIO_ReadFile(const char *file, size_t *size) {
    FILE *f = fopen(file, "rb");
    ASSERT_FMT(f != NULL, "Failed to open %s", file);

    struct stat fs;
    ASSERT_FMT(stat(file, &fs) == 0, "Failed to stat %s", file);

    char *buf = ALLOC_N(fs.st_size, char);
    ASSERT_FMT(buf != NULL, "Failed to alloc %zu bytes", fs.st_size);

    fread(buf, 1, fs.st_size, f);
    ASSERT_FMT(ferror(f) == 0, "Error while reading %s", file);
    ASSERT_FMT(fclose(f) == 0, "Failed to close %s", file);

    *size = fs.st_size;
    return buf;
}
