#ifndef NB_WORLD_VK_H
#define NB_WORLD_VK_H

#include <nbody.h>
#include <stdint.h>
#include <stdbool.h>

/* Constant data given to shaders in a uniform buffer. */
typedef struct WorldData {
    uint32_t total_len; // total number of particles
    uint32_t mass_len;  // number of particles with mass
    float dt;           // time delta
} WorldData;

/* GPU simulation pipeline. */
typedef struct SimPipeline SimPipeline;

/*
 * Setup simulation pipeline for SIZE particles.
 * RES is set to created SimPipeline.
 * MAPPED is set to a host-mapped buffer with latest particle data.
 */
void CreateSimPipeline(SimPipeline **res, void **mapped, uint32_t size);

/* Destroy SIM. */
void DestroySimPipeline(SimPipeline *sim);

/*
 * Perform N > 0 updates with specified time delta.
 * BUFFER_MODIFIED indicates whether particle data in host-mapped buffer was modified since last call.
 */
void PerformSimUpdate(SimPipeline *sim, uint32_t n, WorldData data, bool buffer_modified);

#endif //NB_WORLD_VK_H
