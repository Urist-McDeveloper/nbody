#include <nbody.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

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
    WorldComp *comp;    // Vulkan-related stuff
    bool gpu_sync;      // whether last change in GPU buffer is synced with the array
    bool arr_sync;      // whether last change in the array is synced with GPU buffer
};

/* Copy data from GPU buffer to RAM if necessary. */
static void SyncToArrFromGPU(World *w) {
    if (!w->gpu_sync) {
        WorldComp_GetParticles(w->comp, w->arr);
        w->gpu_sync = true;
        w->arr_sync = true;
    }
}

/* Copy data from RAM to GPU buffer if necessary. */
static void SyncFromArrToGPU(World *w) {
    if (!w->arr_sync) {
        WorldComp_SetParticles(w->comp, w->arr);
        w->gpu_sync = true;
        w->arr_sync = true;
    }
}

World *World_Create(uint32_t size, V2 min, V2 max) {
    World *world = ALLOC(1, World);
    ASSERT_MSG(world != NULL, "Failed to alloc World");

    world->comp = NULL;
    world->gpu_sync = true;
    world->arr_sync = true;

    Particle *arr = ALLOC(size, Particle);
    ASSERT_FMT(arr != NULL, "Failed to alloc %u Particles", size);

    world->arr = arr;
    world->arr_len = size;

    AllocPackArray(size, &world->pack, &world->pack_len);

    #pragma omp parallel for schedule(static, 80) firstprivate(arr, size, min, max) default(none)
    for (int i = 0; i < size; i++) {
        float r = RangeRand(MIN_R, MAX_R);
        float x = RangeRand(min.x + r, max.x - r);
        float y = RangeRand(min.y + r, max.y - r);

        arr[i] = (Particle){0};
        arr[i].pos = V2_From(x, y);
        arr[i].mass = R_TO_M(r);
        arr[i].radius = r;
    }
    return world;
}

void World_Destroy(World *w) {
    if (w != NULL) {
        if (w->comp != NULL) {
            WorldComp_Destroy(w->comp);
        }
        free(w->pack);
        free(w->arr);
        free(w);
    }
}

void World_Update(World *w, float dt, uint32_t n) {
    SyncToArrFromGPU(w);
    w->arr_sync = false;

    Particle *arr = w->arr;
    uint32_t arr_len = w->arr_len;
    ParticlePack *pack = w->pack;
    uint32_t pack_len = w->pack_len;

    for (int update_iter = 0; update_iter < n; update_iter++) {
        PackParticles(w->arr_len, w->arr, w->pack);

        #pragma omp parallel for schedule(static, 20) firstprivate(dt, arr, arr_len, pack, pack_len) default(none)
        for (int i = 0; i < arr_len; i++) {
            PackedUpdate(&arr[i], dt, pack_len, pack);
        }
    }
}

void World_GetParticles(World *w, Particle **ps, uint32_t *size) {
    SyncToArrFromGPU(w);
    *ps = w->arr;
    *size = w->arr_len;
}

void World_InitVK(World *w, const VulkanCtx *ctx) {
    if (w->comp == NULL) {
        WorldData data = (WorldData){
                .size = w->arr_len,
                .dt = 0,
        };
        w->comp = WorldComp_Create(ctx, data);
        w->arr_sync = false;
    }
}

void World_UpdateVK(World *w, float dt, uint32_t n) {
    ASSERT_FMT(w->comp != NULL, "Vulkan has not been initialized for World %p", w);
    SyncFromArrToGPU(w);
    w->gpu_sync = false;
    WorldComp_DoUpdate(w->comp, dt, n);
}
