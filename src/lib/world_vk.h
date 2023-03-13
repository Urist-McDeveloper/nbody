#ifndef RAG_WORLD_VK_H
#define RAG_WORLD_VK_H

#include <rag_vk.h>

/* Constant data given to shaders in a uniform buffer. */
typedef struct WorldData {
    uint32_t size;
    float dt;
} WorldData;

/* Simulation pipeline-related stuff. */
typedef struct WorldComp WorldComp;

/*
 * Initialize necessary Vulkan stuff and setup uniform buffer with WORLD_DATA.
 * Note that WORLD_DATA cannot be changed after initialization.
 *
 * CTX must remain a valid pointer to initialized VulkanCtx until WC is de-initialized.
 */
WorldComp *WorldComp_Create(const VulkanCtx *ctx, WorldData data);

/* De-initialize COMP. */
void WorldComp_Destroy(WorldComp *comp);

/* Perform N updates with specified time delta. */
void WorldComp_DoUpdate(WorldComp *comp, float dt, uint32_t n);

/* Copy bodies from GPU buffer into ARR. */
void WorldComp_GetBodies(WorldComp *comp, Body *arr);

/* Copy bodies from ARR into GPU buffer. */
void WorldComp_SetBodies(WorldComp *comp, Body *arr);

#endif //RAG_WORLD_VK_H
