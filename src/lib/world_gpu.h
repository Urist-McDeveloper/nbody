#ifndef NB_WORLD_VK_H
#define NB_WORLD_VK_H

#include <nbody.h>

#include <stdbool.h>
#include <vulkan/vulkan.h>

/* Constant data given to shaders in a uniform buffer. */
typedef struct WorldData {
    uint32_t size;
    float dt;
} WorldData;

/* GPU simulation pipeline. */
typedef struct SimPipeline SimPipeline;

/*
 * Initialize necessary Vulkan stuff.
 * CTX must remain a valid pointer to initialized VulkanCtx until WC is de-initialized.
 */
SimPipeline *CreateSimPipeline(const VulkanCtx *ctx, WorldData data);

/* Destroy SIM. */
void DestroySimPipeline(SimPipeline *sim);

/*
 * Perform N > 0 updates with specified time delta and write results into ARR.
 * If NEW_DATA is true, then update GPU buffers with data from ARR before running simulation.
 */
void PerformSimUpdate(SimPipeline *sim, uint32_t n, float dt, Particle *arr, bool new_data);

#endif //NB_WORLD_VK_H
