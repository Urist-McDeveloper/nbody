#ifndef NB_VULKAN_CTX_H
#define NB_VULKAN_CTX_H

#include <string.h>
#include <stdbool.h>
#include <vulkan/vulkan.h>

#include "util.h"

/* Global Vulkan context. */
extern struct VulkanContext {
    VkInstance instance;
    VkPhysicalDevice pdev;
    VkDevice dev;
    VkQueue queue;
    VkCommandPool cmd_pool;
    uint32_t queue_family_idx;
} vulkan_ctx;

/*
 * Initialize global Vulkan context the first time this function is called; subsequent calls are ignored.
 * Every other function in this file MUST NOT be called until Vulkan context is initialized.
 */
void InitGlobalVulkanContext();

/* Load shader module from PATH. */
VkShaderModule LoadShaderModule(const char *path);

/* Allocate primary command buffers. */
void AllocCommandBuffers(uint32_t count, VkCommandBuffer *buffers);


/*
 * Memory management.
 */


/* Wrapper of VkBuffer. */
typedef struct VulkanBuffer {
    VkBuffer handle;
    VkDeviceSize size;  // total size (in bytes)
    void *mapped;       // NULL if buffer is not from host-coherent memory
} VulkanBuffer;

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
    if (memory != NULL) vkFreeMemory(vulkan_ctx.dev, memory->handle, NULL);
}

/* Create VulkanBuffer of SIZE bytes. */
VulkanBuffer CreateVulkanBuffer(VulkanDeviceMemory *memory, VkDeviceSize size, VkBufferUsageFlags usage);

/* Destroy BUFFER. */
static inline void DestroyVulkanBuffer(const VulkanBuffer *buffer) {
    if (buffer != NULL) vkDestroyBuffer(vulkan_ctx.dev, buffer->handle, NULL);
}

/*
 * Copy DATA into host-mapped memory of BUFFER.
 * Aborts if BUFFER is not from host-coherent memory.
 */
static inline void CopyIntoVulkanBuffer(const VulkanBuffer *buffer, const void *data) {
    ASSERT_DBG(buffer->mapped != NULL, "Buffer %p is not host-coherent", buffer->handle);
    memcpy(buffer->mapped, data, buffer->size);
}

/*
 * Copy host-mapped memory of BUFFER into DATA.
 * Aborts if BUFFER is not from host-coherent memory.
 */
static inline void CopyFromVulkanBuffer(const VulkanBuffer *buffer, void *data) {
    ASSERT_DBG(buffer->mapped != NULL, "Buffer %p is not host-coherent", buffer->handle);
    memcpy(data, buffer->mapped, buffer->size);
}

/* Copy data from SRC to DST. Both buffers must have the same size. */
void CopyVulkanBuffer(VkCommandBuffer cmd, const VulkanBuffer *src, const VulkanBuffer *dst);

/* Fill INFO. */
void FillDescriptorBufferInfo(const VulkanBuffer *buffer, VkDescriptorBufferInfo *info);

/* Fill buffer memory barrier; src operation is MEMORY_WRITE, dst operation is MEMORY_READ. */
void FillWriteReadBufferBarrier(const VulkanBuffer *buffer, VkBufferMemoryBarrier *barrier);

#endif //NB_VULKAN_CTX_H
