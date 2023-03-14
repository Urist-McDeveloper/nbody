#ifndef NB_VULKAN_CTX_H
#define NB_VULKAN_CTX_H

#include <nbody.h>
#include <vulkan/vulkan.h>

/*
 * VulkanCtx
 */

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
 * VulkanBuffer
 */

/* Wrapper of VkBuffer. */
typedef struct VulkanBuffer {
    VkBuffer handle;
    uint32_t queue_family_idx;
    VkDevice dev;
    VkDeviceMemory memory;
    VkDeviceSize offset;
    VkDeviceSize size;
} VulkanBuffer;

/* Destroy BUFFER. */
void DestroyVulkanBuffer(const VulkanBuffer *buffer);

/* Map BUFFER into address space. */
void *MapVulkanBuffer(const VulkanBuffer *buffer);

/* Unmap previously mapped BUFFER. */
void UnmapVulkanBuffer(const VulkanBuffer *buffer);

/* Copy BUFFER->size bytes from DATA into BUFFER. */
void SetVulkanBufferData(const VulkanBuffer *buffer, const void *data);

/* Copy BUFFER->size bytes from BUFFER into DATA. */
void GetVulkanBufferData(const VulkanBuffer *buffer, void *data);

/* Copy data from SRC to DST. Both buffers must have the same size. */
void CopyVulkanBuffer(VkCommandBuffer cmd, const VulkanBuffer *src, const VulkanBuffer *dst);

/* Fill INFO. */
void FillDescriptorBufferInfo(const VulkanBuffer *buffer, VkDescriptorBufferInfo *info);

/* Fill buffer memory barrier; src operation is MEMORY_WRITE, dst operation is MEMORY_READ. */
void FillVulkanBufferWriteReadBarrier(const VulkanBuffer *buffer, VkBufferMemoryBarrier *barrier);

/*
 * VulkanDeviceMemory
 */

/* Wrapper of VkDeviceMemory. */
typedef struct VulkanDeviceMemory {
    VkDeviceMemory handle;
    VkDevice dev;
    uint32_t queue_family_idx;
    VkDeviceSize size;
    VkDeviceSize used;
} VulkanDeviceMemory;

/* Allocate device memory. FLAGS must not be 0. */
VulkanDeviceMemory CreateDeviceMemory(const VulkanCtx *ctx, VkDeviceSize size, VkMemoryPropertyFlags flags);

/* Destroy MEMORY. */
void DestroyVulkanMemory(const VulkanDeviceMemory *memory);

/* Create VulkanBuffer of SIZE bytes. */
VulkanBuffer CreateVulkanBuffer(VulkanDeviceMemory *memory, VkDeviceSize size, VkBufferUsageFlags usage);

#endif //NB_VULKAN_CTX_H
