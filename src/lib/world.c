#include <nbody.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "sim_cpu.h"
#include "sim_gpu.h"
#include "util.h"

struct World {
    Particle *arr;      // array of particles
    SimPipeline *sim;   // simulation pipeline
    ParticlePack *pack; // array of packed particle data (CPU)
    uint32_t total_len; // total number of particles
    uint32_t mass_len;  // number of particles with mass
    uint32_t pack_len;  // length of pack
    bool gpu_sync;      // whether ARR and GPU buffer hold the same data
};

World *CreateWorld(const Particle *ps, uint32_t size) {
    World *world = ALLOC(1, World);
    ASSERT(world != NULL, "Failed to alloc World");

    SimPipeline *sim;
    Particle *arr;
    CreateSimPipeline(&sim, (void **)&arr, size);

    // copy all particles from PS into arr
    memcpy(arr, ps, size * sizeof(Particle));

    // sort arr so that particles with no mass come after all particles with mass
    uint32_t i = 0, j = size;
    while (true) {
        while (i < j && arr[i].mass > 0) i++;   // arr[i] is the first particle without mass
        while (i < j && arr[--j].mass <= 0);    // arr[j] is the last particle with mass

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
