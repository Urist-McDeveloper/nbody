#include "fio.h"
#include "util.h"

#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#define stat _stat
#endif

void *FIO_ReadFile(const char *file, size_t *size) {
    FILE *f = fopen(file, "rb");
    ASSERT(f != NULL, "Failed to open %s", file);

    struct stat fs;
    ASSERT(stat(file, &fs) == 0, "Failed to stat %s", file);

    char *buf = ALLOC(fs.st_size, char);
    ASSERT(buf != NULL, "Failed to alloc %zu bytes for reading %s", fs.st_size, file);

    fread(buf, 1, fs.st_size, f);
    ASSERT(ferror(f) == 0, "Error while reading %s", file);
    ASSERT(fclose(f) == 0, "Failed to close %s", file);

    *size = fs.st_size;
    return buf;
}
