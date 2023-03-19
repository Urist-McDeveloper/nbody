#ifndef NB_H
#define NB_H

#include <stdint.h>
#include <stdbool.h>

#include <math.h>
#include <vulkan/vulkan.h>

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

/* Vector magnitude. */
static inline float MagV2(V2 v) {
    return hypotf(v.x, v.y);
}

/* Vector magnitude squared. */
static inline float SqMagV2(V2 v) {
    return v.x * v.x + v.y * v.y;
}


/* Simulation particle. */
typedef struct Particle {
    V2 pos, vel, acc;
    float mass, radius;
} Particle;

#if __STDC_VERSION__ >= 201112L
// Vulkan requires structs to be aligned to 16 bytes
_Static_assert(sizeof(Particle) % 16 == 0, "sizeof(Particle) must be a multiple of 16");
#endif


/* The simulated world with fixed particle count. */
typedef struct World World;

/* Create World with SIZE particles copied from PS. Initializes global Vulkan context if necessary. */
World *CreateWorld(const Particle *ps, uint32_t size);

/* Destroy World. */
void DestroyWorld(World *w);

/* Get Particle array and its size. */
const Particle *GetWorldParticles(World *w, uint32_t *size);

/* Perform N updates using CPU simulation. */
void UpdateWorld_CPU(World *w, float dt, uint32_t n);

/* Perform N updates using GPU simulation. */
void UpdateWorld_GPU(World *w, float dt, uint32_t n);


/* Global Vulkan context. */
extern struct VulkanContext {
    VkInstance instance;
    VkPhysicalDevice pdev;
    VkDevice dev;
    VkQueue queue;
    VkCommandPool cmd_pool;
    uint32_t queue_family_idx;
} vk_ctx;

/* Initialize global Vulkan context the first time this function is called; subsequent calls are ignored. */
void InitGlobalVulkanContext(bool need_gfx_queue, const char **instance_ext, uint32_t ext_count);

/* Allocate primary command buffers. */
void AllocCommandBuffers(uint32_t count, VkCommandBuffer *buffers);

#endif //NB_H
