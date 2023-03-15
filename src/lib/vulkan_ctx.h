#ifndef NB_VULKAN_CTX_H
#define NB_VULKAN_CTX_H

#include <nbody.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include "util.h"

struct VulkanCtx {
    VkInstance instance;
    VkPhysicalDevice pdev;
    VkDevice dev;
    VkQueue queue;
    VkCommandPool cmd_pool;
    uint32_t queue_family_idx;
};

/* Load shader module from PATH. */
VkShaderModule LoadVkShaderModule(const VulkanCtx *ctx, const char *path);

/* Allocate primary command buffers. */
void AllocVkCommandBuffers(const VulkanCtx *ctx, uint32_t count, VkCommandBuffer *buffers);


/*
 * Memory management.
 */


/* Wrapper of VkBuffer. */
typedef struct VulkanBuffer {
    const VulkanCtx *ctx;
    VkBuffer handle;
    VkDeviceSize size;  // total size (in bytes)
    void *mapped;       // NULL if buffer is not from host-coherent memory
} VulkanBuffer;

/* Wrapper of VkDeviceMemory capable of linear buffer allocation. */
typedef struct VulkanDeviceMemory {
    const VulkanCtx *ctx;
    VkDeviceMemory handle;
    VkMemoryPropertyFlags flags;
    VkDeviceSize size;              // total size (in bytes)
    VkDeviceSize used;              // how many size bytes are in use
    void *mapped;                   // NULL if memory is not host-coherent
} VulkanDeviceMemory;

/* Allocate device-local memory. */
VulkanDeviceMemory CreateDeviceLocalMemory(const VulkanCtx *ctx, VkDeviceSize size);

/* Allocate host-coherent memory. */
VulkanDeviceMemory CreateHostCoherentMemory(const VulkanCtx *ctx, VkDeviceSize size);

/* Destroy MEMORY. */
static inline void DestroyVulkanMemory(const VulkanDeviceMemory *memory) {
    if (memory != NULL) vkFreeMemory(memory->ctx->dev, memory->handle, NULL);
}

/* Create VulkanBuffer of SIZE bytes. */
VulkanBuffer CreateVulkanBuffer(VulkanDeviceMemory *memory, VkDeviceSize size, VkBufferUsageFlags usage);

/* Destroy BUFFER. */
static inline void DestroyVulkanBuffer(const VulkanBuffer *buffer) {
    if (buffer != NULL) vkDestroyBuffer(buffer->ctx->dev, buffer->handle, NULL);
}

/*
 * Copy DATA into host-mapped memory of BUFFER.
 * Aborts if BUFFER is not from host-coherent memory.
 */
static inline void CopyIntoVulkanBuffer(const VulkanBuffer *buffer, const void *data) {
    ASSERT(buffer->mapped != NULL, "Buffer %p is not host-coherent", buffer->handle);
    memcpy(buffer->mapped, data, buffer->size);
}

/*
 * Copy host-mapped memory of BUFFER into DATA.
 * Aborts if BUFFER is not from host-coherent memory.
 */
static inline void CopyFromVulkanBuffer(const VulkanBuffer *buffer, void *data) {
    ASSERT(buffer->mapped != NULL, "Buffer %p is not host-coherent", buffer->handle);
    memcpy(data, buffer->mapped, buffer->size);
}

/* Copy data from SRC to DST. Both buffers must have the same size. */
void CopyVulkanBuffer(VkCommandBuffer cmd, const VulkanBuffer *src, const VulkanBuffer *dst);

/* Fill INFO. */
void FillDescriptorBufferInfo(const VulkanBuffer *buffer, VkDescriptorBufferInfo *info);

/* Fill buffer memory barrier; src operation is MEMORY_WRITE, dst operation is MEMORY_READ. */
void FillWriteReadBufferBarrier(const VulkanBuffer *buffer, VkBufferMemoryBarrier *barrier);

#endif //NB_VULKAN_CTX_H
