#ifndef NB_WORLD_VK_H
#define NB_WORLD_VK_H

#include <nbody.h>

#include <stdbool.h>
#include <vulkan/vulkan.h>

/* Constant data given to shaders in a uniform buffer. */
typedef struct WorldData {
    uint32_t total_len; // total number of particles
    uint32_t mass_len;  // number of particles with mass
    float dt;           // time delta
} WorldData;

/* GPU simulation pipeline. */
typedef struct SimPipeline SimPipeline;

/*
 * Initialize necessary Vulkan stuff.
 * Global Vulkan context MUST be initialized prior to calling this function.
 */
SimPipeline *CreateSimPipeline(WorldData data);

/* Destroy SIM. */
void DestroySimPipeline(SimPipeline *sim);

/*
 * Perform N > 0 updates with specified time delta and write results into ARR.
 * If NEW_DATA is true, then update GPU buffers with data from ARR before running simulation.
 */
void PerformSimUpdate(SimPipeline *sim, uint32_t n, float dt, Particle *arr, bool new_data);

#endif //NB_WORLD_VK_H
