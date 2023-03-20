#ifndef NB_VULKAN_CTX_H
#define NB_VULKAN_CTX_H

#include <string.h>
#include <stdbool.h>

#include <nbody.h>
#include "util.h"

/* Wrapper of VkDeviceMemory capable of linear buffer allocation. */
typedef struct VulkanDeviceMemory {
    VkDeviceMemory handle;
    VkDeviceSize size;              // total size (in bytes)
    VkDeviceSize used;              // how many size bytes are in use
    void *mapped;                   // NULL if memory is not host-coherent
} VulkanDeviceMemory;

/* Allocate device-local memory. */
VulkanDeviceMemory CreateDeviceLocalMemory(VkDeviceSize size);

/* Allocate host-coherent memory. */
VulkanDeviceMemory CreateHostCoherentMemory(VkDeviceSize size);

/* Destroy MEMORY. */
static inline void DestroyVulkanMemory(const VulkanDeviceMemory *memory) {
    if (memory != NULL) vkFreeMemory(vk_ctx.dev, memory->handle, NULL);
}

/* Create VulkanBuffer of SIZE bytes. */
VulkanBuffer CreateVulkanBuffer(VulkanDeviceMemory *memory, VkDeviceSize size, VkBufferUsageFlags usage);

/* Destroy BUFFER. */
static inline void DestroyVulkanBuffer(const VulkanBuffer *buffer) {
    if (buffer != NULL) vkDestroyBuffer(vk_ctx.dev, buffer->handle, NULL);
}

/*
 * Copy DATA into host-mapped memory of BUFFER.
 * BUFFER must have been created from host-coherent memory.
 */
static inline void CopyIntoVulkanBuffer(const VulkanBuffer *buffer, const void *data) {
    ASSERT_DBG(buffer->mapped != NULL, "Buffer %p is not host-coherent", buffer->handle);
    memcpy(buffer->mapped, data, buffer->size);
}

/*
 * Copy host-mapped memory of BUFFER into DATA.
 * BUFFER must have been created from host-coherent memory.
 */
static inline void CopyFromVulkanBuffer(const VulkanBuffer *buffer, void *data) {
    ASSERT_DBG(buffer->mapped != NULL, "Buffer %p is not host-coherent", buffer->handle);
    memcpy(data, buffer->mapped, buffer->size);
}

/* Copy data from SRC to DST. Both buffers must have the same size. */
void CopyVulkanBuffer(VkCommandBuffer cmd, const VulkanBuffer *src, const VulkanBuffer *dst);

#endif //NB_VULKAN_CTX_H
