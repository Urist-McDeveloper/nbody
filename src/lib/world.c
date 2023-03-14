#include <nbody.h>

#include <stdlib.h>
#include <stdbool.h>

#include "particle_pack.h"
#include "util.h"
#include "world_vk.h"

/* Minimum radius of randomized particles. */
#define MIN_R   2.0f

/* Maximum radius of randomized particles. */
#define MAX_R   2.0f

/* Density of a Particles (used to calculate mass from radius). */
#define DENSITY 1.0f

/* Homegrown constants are the best. */
#define PI  3.14159274f

/* Convert radius to mass (R is evaluated 3 times). */
#define R_TO_M(R)   ((4.f * PI * DENSITY / 3.f) * (R) * (R) * (R))

/* Get random float in range [MIN, MAX). */
static float RangeRand(float min, float max) {
    return min + (max - min) * ((float)rand() / (float)RAND_MAX);
}

struct World {
    Particle *arr;      // array of particles
    ParticlePack *pack; // array of packed particle data
    uint32_t arr_len;   // length of arr
    uint32_t pack_len;  // length of pack
    SimPipeline *comp;  // Vulkan-related stuff
    bool arr_gpu_sync;  // whether ARR and GPU buffer hold the same data
};

World *CreateWorld(uint32_t size, V2 min, V2 max) {
    World *world = ALLOC(1, World);
    ASSERT_MSG(world != NULL, "Failed to alloc World");

    world->comp = NULL;             // must be explicitly initialized by calling SetupWorldGPU
    world->arr_gpu_sync = false;    // GPU buffers are uninitialized

    world->arr_len = size;
    world->arr = ALLOC(size, Particle);
    ASSERT_FMT(world->arr != NULL, "Failed to alloc %u Particles", size);

    AllocPackArray(size, &world->pack, &world->pack_len);

    #pragma omp parallel for schedule(static, 20) firstprivate(world, size, min, max) default(none)
    for (int i = 0; i < size; i++) {
        float r = RangeRand(MIN_R, MAX_R);
        float x = RangeRand(min.x + r, max.x - r);
        float y = RangeRand(min.y + r, max.y - r);

        world->arr[i] = (Particle){0};
        world->arr[i].pos = V2_FROM(x, y);
        world->arr[i].mass = R_TO_M(r);
        world->arr[i].radius = r;
    }
    return world;
}

void DestroyWorld(World *w) {
    if (w != NULL) {
        if (w->comp != NULL) {
            DestroySimPipeline(w->comp);
        }
//        free(w->pack);
        free(w->arr);
        free(w);
    }
}

void UpdateWorld_CPU(World *w, float dt, uint32_t n) {
    for (int update_iter = 0; update_iter < n; update_iter++) {
        PackParticles(w->arr_len, w->arr, w->pack);

        #pragma omp parallel for schedule(static, 20) firstprivate(dt, w) default(none)
        for (int i = 0; i < w->arr_len; i++) {
            PackedUpdate(&w->arr[i], dt, w->pack_len, w->pack);
        }
    }
    w->arr_gpu_sync = false;
}

void GetWorldParticles(World *w, Particle **ps, uint32_t *size) {
    *ps = w->arr;
    *size = w->arr_len;
}

void SetupWorldGPU(World *w, const VulkanCtx *ctx) {
    if (w->comp == NULL) {
        WorldData data = (WorldData){
                .size = w->arr_len,
                .dt = 0,
        };
        w->comp = CreateSimPipeline(ctx, data);
    }
}

void UpdateWorld_GPU(World *w, float dt, uint32_t n) {
    ASSERT_FMT(w->comp != NULL, "Vulkan has not been initialized for World %p", w);
    if (n > 0) {
        PerformSimUpdate(w->comp, n, dt, w->arr, !w->arr_gpu_sync);
        w->arr_gpu_sync = true;
    }
}
