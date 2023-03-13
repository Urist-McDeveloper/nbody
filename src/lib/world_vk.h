#ifndef NB_WORLD_VK_H
#define NB_WORLD_VK_H

#include <nbody.h>

/* Constant data given to shaders in a uniform buffer. */
typedef struct WorldData {
    uint32_t size;
    float dt;
} WorldData;

/* Simulation pipeline-related stuff. */
typedef struct SimPipeline SimPipeline;

/*
 * Initialize necessary Vulkan stuff.
 * CTX must remain a valid pointer to initialized VulkanCtx until WC is de-initialized.
 */
SimPipeline *CreateSimPipeline(const VulkanCtx *ctx, WorldData data);

/* Destroy SIM. */
void DestroySimPipeline(SimPipeline *sim);

/* Perform N updates with specified time delta. */
void PerformSimUpdate(SimPipeline *sim, float dt, uint32_t n);

/* Copy particles from GPU buffer into ARR. */
void GetSimParticles(SimPipeline *sim, Particle *arr);

/* Copy particles from ARR into GPU buffer. */
void SetSimParticles(SimPipeline *sim, Particle *arr);

#endif //NB_WORLD_VK_H
