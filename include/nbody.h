#ifndef NB_H
#define NB_H

#include <stdint.h>
#include <vulkan/vulkan.h>

/*
 * Gravitational constant; gravity is proportional to the inverse square of distance.
 *      g = NB_G * mass / dist^2
 */
#define NB_G    10.0f

/*
 * Repulsion constant; repulsion is proportional to the inverse cube of distance.
 *      n = NB_N * mass / dist^3
 */
#define NB_N    (-0.f)

/*
 * A fraction of velocity that becomes deceleration.
 *      f = NB_F * velocity
 */
#define NB_F    (-0.f)


/* Vulkan context. */
typedef struct VulkanCtx VulkanCtx;

/* Initialize CTX. */
VulkanCtx *CreateVulkanCtx();

/* De-initialize CTX. */
void DestroyVulkanCtx(VulkanCtx *ctx);


/* 2D vector of floats. */
typedef struct V2 {
    float x, y;
} V2;

/* Zero-length vector. */
#define V2_ZERO         (V2){ .x = 0.0, .y = 0.0 }

/* Constructor macro. */
#define V2_FROM(X, Y)   (V2){ .x = (X), .y = (Y) }

/* Vector addition. */
static inline V2 AddV2(V2 a, V2 b) {
    return V2_FROM(a.x + b.x, a.y + b.y);
}

/* Scalar multiplication. */
static inline V2 ScaleV2(V2 v, float f) {
    return V2_FROM(v.x * f, v.y * f);
}

/* Simulation particle. */
typedef struct Particle {
    V2 pos, vel, acc;
    float mass, radius;
} Particle;

// Vulkan requires structs to be aligned on 16 bytes
_Static_assert(sizeof(Particle) == 32, "sizeof(Particle) must be 32");

/* The simulated world with fixed particle count. */
typedef struct World World;

/* Create World of given SIZE and randomize particle positions within MIN and MAX. */
World *CreateWorld(uint32_t size, V2 min, V2 max);

/* Destroy World. */
void DestroyWorld(World *w);

/* Perform N updates using CPU simulation. */
void UpdateWorld_CPU(World *w, float dt, uint32_t n);

/* Get Particle array and its size. */
void GetWorldParticles(World *w, Particle **ps, uint32_t *size);

/*
 * Setup Vulkan pipeline for W. Does nothing if Vulkan was already set up.
 * CTX must remain a valid pointer to VulkanCtx until W is destroyed.
 */
void SetupWorldGPU(World *w, const VulkanCtx *ctx);

/*
 * Perform N updates using GPU simulation.
 * Aborts if Vulkan has not been setup for W.
 */
void UpdateWorld_GPU(World *w, float dt, uint32_t n);

#endif //NB_H
