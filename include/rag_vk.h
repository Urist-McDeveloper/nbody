#ifndef RAG_VK_H
#define RAG_VK_H

#include "rag.h"

#include <stdbool.h>
#include <vulkan/vulkan.h>

/* Vulkan context. */
typedef struct VulkanCtx {
    VkInstance instance;
    VkPhysicalDevice pdev;
    VkDevice dev;
    VkQueue queue;
    VkCommandPool cmd_pool;
} VulkanCtx;

/* Initialize CTX. */
void VulkanCtx_Init(VulkanCtx *ctx, bool need_gfx_queue);

/* De-initialize VulkanCtx. */
void VulkanCtx_DeInit(VulkanCtx *ctx);

/* Load shader module from PATH. */
VkShaderModule VulkanCtx_LoadShader(const VulkanCtx *ctx, const char *path);

/*
 * Setup Vulkan resources for W. Updates will use constant time delta DT.
 * CTX must remain a valid pointer to initialized VulkanCtx until W is destroyed.
 */
void World_InitVK(World *w, const VulkanCtx *ctx, float dt);

#endif //RAG_VK_H
