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

/* Global Vulkan context. */
extern struct VulkanContext {
    VkInstance instance;
    VkPhysicalDevice pdev;
    VkDevice dev;
    VkQueue queue;
    VkCommandPool cmd_pool;
    uint32_t queue_family_idx;
} vk_ctx;

/* Wrapper of VkBuffer. */
typedef struct VulkanBuffer {
    VkBuffer handle;
    VkDeviceSize size;  // total size (in bytes)
    void *mapped;       // NULL if buffer is not from host-coherent memory
} VulkanBuffer;

/* Initialize global Vulkan context the first time this function is called; subsequent calls are ignored. */
void InitGlobalVulkanContext(bool need_gfx_queue, const char **instance_ext, uint32_t ext_count);

/* Fill INFO with metadata of BUFFER. */
void FillDescriptorBufferInfo(const VulkanBuffer *buffer, VkDescriptorBufferInfo *info);

/* Allocate primary command buffers. */
void AllocCommandBuffers(uint32_t count, VkCommandBuffer *buffers);

static inline void FreeCommandBuffers(uint32_t count, VkCommandBuffer *buffers) {
    vkFreeCommandBuffers(vk_ctx.dev, vk_ctx.cmd_pool, count, buffers);
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

/* Get device-local buffer of latest particle data. */
const VulkanBuffer *GetWorldParticleBuffer(World *w);

/* Perform N updates using CPU simulation. */
void UpdateWorld_CPU(World *w, float dt, uint32_t n);

/* Perform N updates using GPU simulation. */
void UpdateWorld_GPU(World *w, VkEvent set_event, float dt, uint32_t n);

#endif //NB_H
