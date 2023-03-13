#ifndef NB_UTIL_H
#define NB_UTIL_H

#include <stdlib.h>     // malloc, realloc, abort
#include <stdio.h>      // fprintf, stderr
#include <errno.h>      // errno
#include <string.h>     // strerror

/* Allocate N*sizeof(T) bytes. */
#define ALLOC(N, T)             (T*)malloc((N) * sizeof(T))

/* Allocate N*sizeof(T) bytes; returned pointer is guaranteed to be multiple of A. */
#define ALLOC_ALIGNED(A, N, T)  (T*)aligned_alloc(A, (N) * sizeof(T))

/* Print error message and abort if COND is false. */
#define ASSERT_MSG(COND, MSG)   ASSERT_FMT(COND, MSG, NULL)

/* Print error message and abort if COND is false. */
#define ASSERT_FMT(COND, MSG, ...)                                          \
    do {                                                                    \
        if (!(COND)) {                                                      \
            fprintf(stderr, "%s:%d [%s] errno = %d, str = %s\n",            \
                    __FILE__, __LINE__, __func__, errno, strerror(errno));  \
            fprintf(stderr, "%s:%d [%s] " MSG "\n",                         \
                    __FILE__, __LINE__, __func__, __VA_ARGS__);             \
            abort();                                                        \
        }                                                                   \
    } while (0)

#ifdef VULKAN_H_

/* Assert that Vulkan library function returned VK_SUCCESS. */
#define ASSERT_VKR(X, MSG) util_assert_vkr(X, MSG, __FILE__, __LINE__, __func__)

static inline const char *util_vkr_to_str(VkResult x) {
    switch (x) {
        case VK_SUCCESS:
            return "VK_SUCCESS";
        case VK_NOT_READY:
            return "VK_NOT_READY";
        case VK_TIMEOUT:
            return "VK_TIMEOUT";
        case VK_EVENT_SET:
            return "VK_EVENT_SET";
        case VK_EVENT_RESET:
            return "VK_EVENT_RESET";
        case VK_INCOMPLETE:
            return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:
            return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL:
            return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN:
            return "VK_ERROR_UNKNOWN";
        default:
            return "Unknown VkResult";
    }
}

static inline void util_assert_vkr(VkResult x, const char *msg, const char *file, int line, const char *func) {
    if (x != VK_SUCCESS) {
        fprintf(stderr, "%s:%d [%s] VkResult = %d, str = %s\n",
                file, line, func, x, util_vkr_to_str(x));
        fprintf(stderr, "%s:%d [%s] %s\n",
                file, line, func, msg);
        abort();
    }
}

#endif //VULKAN_H_
#endif //NB_UTIL_H
