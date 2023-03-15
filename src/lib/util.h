#ifndef NB_UTIL_H
#define NB_UTIL_H

#include <stdlib.h>     // malloc, realloc, abort
#include <stdio.h>      // fprintf, stderr
#include <errno.h>      // errno
#include <string.h>     // strerror

/* Allocate `N * sizeof(T)` bytes. */
#define ALLOC(N, T)             (T*)malloc((N) * sizeof(T))

/* Print error message and abort if COND is false. */
#define ASSERT(COND, ...)                                                   \
    do {                                                                    \
        if (!(COND)) {                                                      \
            fprintf(stderr, "%s:%d [%s] errno = %d, str = %s\n",            \
                    __FILE__, __LINE__, __func__, errno, strerror(errno));  \
            fprintf(stderr, "%s:%d [%s] ", __FILE__, __LINE__, __func__);   \
            fprintf(stderr, __VA_ARGS__);                                   \
            fprintf(stderr, "\n");                                          \
            abort();                                                        \
        }                                                                   \
    } while (0)

#ifndef NDEBUG
#   define ASSERT_DBG(COND, ...)   ASSERT(COND, __VA_ARGS__)
#else
#   define ASSERT_DBG(COND, ...)   (void)(COND)
#endif

#endif //NB_UTIL_H

#ifdef VULKAN_H_
#ifndef NB_UTIL_VK_H
#define NB_UTIL_VK_H

/* 16-byte aligned sizeof. */
#define SIZE_OF_ALIGN_16(T)     (sizeof(T) + (sizeof(T) % 16 == 0 ? 0 : 16 - (sizeof(T) % 16)))

/* Print error message and abort if X is not VK_SUCCESS. */
#define ASSERT_VK(X, ...)                                                                   \
    do {                                                                                    \
        VkResult util_assert_vk_x = (X);                                                    \
        if (util_assert_vk_x != VK_SUCCESS) {                                               \
            fprintf(stderr, "%s:%d [%s] VkResult = %d, str = %s\n",                         \
                    __FILE__, __LINE__, __func__,                                           \
                    util_assert_vk_x,                                                       \
                    util_vkr_to_str(util_assert_vk_x));                                     \
            fprintf(stderr, "%s:%d [%s] ", __FILE__, __LINE__, __func__);                   \
            fprintf(stderr, __VA_ARGS__);                                                   \
            fprintf(stderr, "\n");                                                          \
            abort();                                                                        \
        }                                                                                   \
    } while (0)

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

#endif //NB_UTIL_VK_H
#endif //VULKAN_H_
