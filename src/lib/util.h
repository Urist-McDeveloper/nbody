#ifndef RAG_UTIL_H
#define RAG_UTIL_H

#include <stdlib.h>     // malloc, realloc, abort
#include <stdio.h>      // fprintf, stderr, perror
#include <errno.h>
#include <stdbool.h>

/* Allocate sizeof(T) bytes. */
#define ALLOC(T)            ALLOC_N(1, T)

/* Allocate N*sizeof(T) bytes. */
#define ALLOC_N(N, T)       (T*)malloc((N) * sizeof(T))

/* Reallocate P with N*sizeof(T) bytes. */
#define REALLOC(P, N, T)    (T*)realloc(P, (N) * sizeof(T))

/* Abort if COND is false. */
#define ASSERT(COND)        assert_or_abort(COND, #COND, __FILE__, __LINE__, __FUNCTION__)

#ifdef VULKAN_H_
/* Assert that Vulkan library function returned VK_SUCCESS. */
#define ASSERT_VK(X) ASSERT((X) == VK_SUCCESS)
#endif

static inline void assert_or_abort(bool cond, const char *expr, const char *file, int line, const char *func) {
    if (!cond) {
        if (errno != 0) {
            perror(expr);
        }
        fprintf(stderr, "%s:%d [%s] Assertion failed: %s\n", file, line, func, expr);
        abort();
    }
}

#endif //RAG_UTIL_H
