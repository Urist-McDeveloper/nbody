#ifndef RAG_ERR_H
#define RAG_ERR_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/* Call perror(NULL) and abort if COND is false. */
#define ASSERT(COND) assert_or_abort(COND, __FILE_NAME__, __LINE__, __FUNCTION__)

static void assert_or_abort(bool cond, const char *file, int line, const char *function) {
    if (!cond) {
        fprintf(stderr, "%s:%d [%s]\n", file, line, function);
        perror(NULL);
        abort();
    }
}

#endif //RAG_ERR_H
