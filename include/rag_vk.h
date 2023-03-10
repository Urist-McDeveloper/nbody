#ifndef RAG_VK_H
#define RAG_VK_H

#include "rag.h"

#include <stdbool.h>
#include <vulkan/vulkan.h>

/* 16-byte aligned sizeof. */
#define VK_SIZE_OF_16(T) (sizeof(T) + (sizeof(T) % 16))

/* Vulkan context. */
typedef struct VulkanCtx {
    VkInstance instance;
    VkPhysicalDevice pdev;
    VkDevice dev;
    VkQueue queue;
    VkCommandPool cmd_pool;
    uint32_t queue_family_idx;
} VulkanCtx;

/* Initialize CTX. */
void VulkanCtx_Init(VulkanCtx *ctx, bool need_gfx_queue);

/* De-initialize VulkanCtx. */
void VulkanCtx_DeInit(VulkanCtx *ctx);

/* Load shader module from PATH. */
VkShaderModule VulkanCtx_LoadShader(const VulkanCtx *ctx, const char *path);

/* Allocate primary command buffers. */
void VulkanCtx_AllocCommandBuffers(const VulkanCtx *ctx, uint32_t count, VkCommandBuffer *buffers);

/* Allocate device memory. FLAGS must not be 0. */
VkDeviceMemory VulkanCtx_AllocMemory(const VulkanCtx *ctx, VkDeviceSize size, VkMemoryPropertyFlags flags);

/* Create exclusive buffer. */
VkBuffer VulkanCtx_CreateBuffer(const VulkanCtx *ctx, VkDeviceSize size, VkBufferUsageFlags usage);

/*
 * Setup Vulkan resources for W. Updates will use constant time delta DT.
 * CTX must remain a valid pointer to initialized VulkanCtx until W is destroyed.
 */
void World_InitVK(World *w, const VulkanCtx *ctx, float dt);

#endif //RAG_VK_H
