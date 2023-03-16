#ifndef NB_WORLD_VK_H
#define NB_WORLD_VK_H

#include <nbody.h>
#include <stdint.h>

/* Constant data given to shaders in a uniform buffer. */
typedef struct WorldData {
    uint32_t total_len; // total number of particles
    uint32_t mass_len;  // number of particles with mass
    float dt;           // time delta
} WorldData;

/* GPU simulation pipeline. */
typedef struct SimPipeline SimPipeline;

/*
 * Setup simulation pipeline.
 * Only `dt` field of DATA can be changed later.
 */
SimPipeline *CreateSimPipeline(WorldData data);

/* Destroy simulation pipeline. */
void DestroySimPipeline(SimPipeline *sim);

/* Copy particle data from GPU buffer into PS. */
void GetSimulationData(const SimPipeline *sim, Particle *ps);

/* Copy particle data from PS into GPU buffer. */
void SetSimulationData(SimPipeline *sim, const Particle *ps);

/*
 * Perform N > 0 updates with time step.
 * Simulation data MUST have been set prior to calling this function.
 */
void PerformSimUpdate(SimPipeline *sim, uint32_t n, float dt);

#endif //NB_WORLD_VK_H
