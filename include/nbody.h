#ifndef NB_H
#define NB_H

#include <stdint.h>

/* Gravitational constant; `g = NB_G * mass / dist^2`. */
#define NB_G    10.0f

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

/* Vector subtraction. */
static inline V2 SubV2(V2 a, V2 b) {
    return V2_FROM(a.x - b.x, a.y - b.y);
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

// Vulkan requires structs to be aligned to 16 bytes
_Static_assert(sizeof(Particle) % 16 == 0, "sizeof(Particle) must be a multiple of 16");

/* The simulated world with fixed particle count. */
typedef struct World World;

/* Create World with SIZE particles copied from PS. */
World *CreateWorld(const Particle *ps, uint32_t size);

/* Destroy World. */
void DestroyWorld(World *w);

/* Get Particle array and its size. */
const Particle *GetWorldParticles(World *w, uint32_t *size);

/* Perform N updates using CPU simulation. */
void UpdateWorld_CPU(World *w, float dt, uint32_t n);

/* Perform N updates using GPU simulation. */
void UpdateWorld_GPU(World *w, float dt, uint32_t n);

#endif //NB_H
