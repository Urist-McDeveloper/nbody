#ifndef NB_FIO_H
#define NB_FIO_H

#include <stddef.h>

/*
 * Fully read PATH as binary file and return its content.
 * Content length (in bytes) is stored in SIZE.
 */
void *FIO_ReadFile(const char *path, size_t *size);

#endif //NB_FIO_H
