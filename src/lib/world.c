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
    ParticlePack *pack; // array of packed particle data
    uint32_t total_len; // total number of particles
    uint32_t mass_len;  // number of particles with mass
    uint32_t pack_len;  // length of pack
    bool arr_sync;      // whether latest change in ARR is synced with GPU buffer
    bool gpu_sync;      // whether latest change in GPU buffer is synced with ARR
};

World *CreateWorld(const Particle *ps, uint32_t size) {
    World *world = ALLOC(1, World);
    ASSERT(world != NULL, "Failed to alloc World");

    Particle *arr = ALLOC(size, Particle);
    ASSERT(arr != NULL, "Failed to alloc %u particles", size);

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
    // j == index of the first particle without mass == number of particles with mass

    WorldData world_data = {
            .total_len = size,
            .mass_len = j,
    };
    SimPipeline *sim = CreateSimPipeline(world_data);

    *world = (World){
        .arr = arr,
        .sim = sim,
        .total_len = size,
        .mass_len = j,
        .arr_sync = false,  // ARR must be synced with GPU buffer
        .gpu_sync = true,   // GPU buffer have no data to sync
    };
    AllocPackArray(&world->pack, &world->pack_len, world->mass_len);

    return world;
}

void DestroyWorld(World *w) {
    if (w != NULL) {
        DestroySimPipeline(w->sim);
        FreePackArray(w->pack);
        free(w->pack);
        free(w);
    }
}

/* Sync changes from ARR to GPU buffer, if necessary. */
static void SyncFromArrToGPU(World *w) {
    if (!w->arr_sync) {
        SetSimulationData(w->sim, w->arr);
        w->arr_sync = true;
    }
}

/* Sync changes from GPU buffer to ARR, if necessary. */
static void SyncToArrFromGPU(World *w) {
    if (!w->gpu_sync) {
        GetSimulationData(w->sim, w->arr);
        w->gpu_sync = true;
    }
}

const Particle *GetWorldParticles(World *w, uint32_t *size) {
    SyncToArrFromGPU(w);
    if (size != NULL) {
        *size = w->total_len;
    }
    return w->arr;
}

void UpdateWorld_CPU(World *w, float dt, uint32_t n) {
    SyncToArrFromGPU(w);
    for (uint32_t update_iter = 0; update_iter < n; update_iter++) {
        PackParticles(w->mass_len, w->arr, w->pack);

        #pragma omp parallel for schedule(static, 20) firstprivate(dt, w) default(none)
        for (uint32_t i = 0; i < w->total_len; i++) {
            PackedUpdate(&w->arr[i], dt, w->pack_len, w->pack);
        }
    }
    w->arr_sync = false;
}

void UpdateWorld_GPU(World *w, float dt, uint32_t n) {
    if (n > 0) {
        SyncFromArrToGPU(w);
        PerformSimUpdate(w->sim, n, dt);
        w->gpu_sync = false;
    }
}
