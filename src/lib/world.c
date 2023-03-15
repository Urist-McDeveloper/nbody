#include <nbody.h>

#include <stdlib.h>
#include <stdbool.h>

#include "sim_cpu.h"
#include "sim_gpu.h"
#include "util.h"

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
    SimPipeline *sim;   // simulation pipeline
    ParticlePack *pack; // array of packed particle data (CPU)
    uint32_t total_len; // total number of particles
    uint32_t mass_len;  // number of particles with mass
    uint32_t pack_len;  // length of pack
    bool gpu_sync;      // whether ARR and GPU buffer hold the same data
};

World *CreateWorld(uint32_t size, V2 min, V2 max) {
    World *world = ALLOC(1, World);
    ASSERT(world != NULL, "Failed to alloc World");

    SimPipeline *sim;
    Particle *arr;
    CreateSimPipeline(&sim, (void **)&arr, size);

    #pragma omp parallel for schedule(static, 20) firstprivate(arr, size, min, max) default(none)
    for (uint32_t i = 0; i < size; i++) {
        arr[i] = (Particle){0};

        // make 90% of all particles massless
        if (rand() < (RAND_MAX / 10)) {
            arr[i].radius = RangeRand(MIN_R, MAX_R);
            arr[i].mass = R_TO_M(arr[i].radius);
        } else {
            arr[i].radius = 1.f;
            arr[i].mass = 0.f;
        }

        float x = RangeRand(min.x + arr[i].radius, max.x - arr[i].radius);
        float y = RangeRand(min.y + arr[i].radius, max.y - arr[i].radius);
        arr[i].pos = V2_FROM(x, y);
    }

    uint32_t i = 0, j = size;
    while (true) {
        while (i < j && arr[i].mass != 0) i++;  // arr[i] is the first particle without mass
        while (i < j && arr[--j].mass == 0);    // arr[j] is the last particle with mass

        // if i == j then array is sorted
        if (i == j) break;

        // swap arr[i] and arr[j]
        Particle tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }

    *world = (World){
        .arr = arr,
        .sim = sim,
        .total_len = size,
        .mass_len = j,      // j == index of the first particle without mass == number of particles with mass
        .gpu_sync = false,  // GPU buffers are uninitialized
    };
    AllocPackArray(&world->pack, &world->pack_len, world->mass_len);

    return world;
}

void DestroyWorld(World *w) {
    if (w != NULL) {
        DestroySimPipeline(w->sim);
        free(w->pack);
        free(w);
    }
}

const Particle *GetWorldParticles(World *w, uint32_t *size) {
    *size = w->total_len;
    return w->arr;
}

void UpdateWorld_CPU(World *w, float dt, uint32_t n) {
    for (uint32_t update_iter = 0; update_iter < n; update_iter++) {
        PackParticles(w->mass_len, w->arr, w->pack);

        #pragma omp parallel for schedule(static, 20) firstprivate(dt, w) default(none)
        for (uint32_t i = 0; i < w->total_len; i++) {
            PackedUpdate(&w->arr[i], dt, w->pack_len, w->pack);
        }
    }
    w->gpu_sync = false;
}

void UpdateWorld_GPU(World *w, float dt, uint32_t n) {
    ASSERT(w->sim != NULL, "Vulkan has not been initialized for World %p", w);
    if (n > 0) {
        WorldData data = (WorldData){
            .total_len = w->total_len,
            .mass_len = w->mass_len,
            .dt = dt,
        };
        PerformSimUpdate(w->sim, n, data, !w->gpu_sync);
        w->gpu_sync = true;
    }
}
