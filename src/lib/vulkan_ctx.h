#ifndef NB_VULKAN_CTX_H
#define NB_VULKAN_CTX_H

#include <nbody.h>

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

/* Allocate device memory. FLAGS must not be 0. */
VkDeviceMemory AllocVkDeviceMemory(const VulkanCtx *ctx, VkDeviceSize size, VkMemoryPropertyFlags flags);

/* Create exclusive buffer. */
VkBuffer CreateVkBuffer(const VulkanCtx *ctx, VkDeviceSize size, VkBufferUsageFlags usage);

#endif //NB_VULKAN_CTX_H
