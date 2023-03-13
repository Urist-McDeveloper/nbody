#include <rag.h>
#include <rag_vk.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "util.h"
#include "world_vk.h"

/* Minimum radius of randomized Body. */
#define MIN_R   2.0f

/* Maximum radius of randomized Body. */
#define MAX_R   2.0f

/* Density of a Body (used to calculate mass from radius). */
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
    Body *arr;          // array of Bodies
    int size;           // length of the array
    WorldComp *comp;    // Vulkan-related stuff
    bool gpu_sync;      // whether last change in GPU buffer is synced with the array
    bool arr_sync;      // whether last change in the array is synced with GPU buffer
};

/* Copy data from GPU buffer to RAM if necessary. */
static void SyncToArrFromGPU(World *w) {
    if (!w->gpu_sync) {
        WorldComp_GetBodies(w->comp, w->arr);
        w->gpu_sync = true;
        w->arr_sync = true;
    }
}

/* Copy data from RAM to GPU buffer if necessary. */
static void SyncFromArrToGPU(World *w) {
    if (!w->arr_sync) {
        WorldComp_SetBodies(w->comp, w->arr);
        w->gpu_sync = true;
        w->arr_sync = true;
    }
}

World *World_Create(int size, V2 min, V2 max) {
    World *world = ALLOC(World);
    ASSERT_MSG(world != NULL, "Failed to alloc World");

    Body *arr = ALLOC_N(size, Body);
    ASSERT_FMT(arr != NULL, "Failed to alloc %d Body", size);

    for (int i = 0; i < size; i++) {
        float r = RangeRand(MIN_R, MAX_R);
        float x = RangeRand(min.x + r, max.x - r);
        float y = RangeRand(min.y + r, max.y - r);

        arr[i].pos = V2_From(x, y);
        arr[i].vel = V2_ZERO;
        arr[i].acc = V2_ZERO;
        arr[i].mass = R_TO_M(r);
        arr[i].radius = r;
    }

    *world = (World){
            .arr = arr,
            .size = size,
            .comp = NULL,
            .gpu_sync = true,
            .arr_sync = true,
    };
    return world;
}

void World_Destroy(World *w) {
    if (w != NULL) {
        if (w->comp != NULL) {
            WorldComp_Destroy(w->comp);
        }
        free(w->arr);
        free(w);
    }
}

void World_Update(World *w, float dt) {
    SyncToArrFromGPU(w);
    w->arr_sync = false;

    Body *arr = w->arr;
    int size = w->size;

    #pragma omp parallel for firstprivate(arr, size) default(none)
    for (int i = 0; i < size; i++) {
        Body this = arr[i];

        // initial acceleration is friction
        this.acc = V2_Mul(this.vel, RAG_FRICTION);

        for (int j = 0; j < size; j++) {
            if (i == j) continue;
            Body that = arr[j];

            V2 radv = V2_Sub(that.pos, this.pos);
            float dist = V2_Mag(radv);

            float min_dist = 0.5f * (this.radius + that.radius);
            if (dist < min_dist) {
                dist = min_dist;
            }

            //          g  ==  Gm / r^2
            //          n  ==  Nm / r^3
            // norm(radv)  ==  radv * (1 / r)
            //
            // norm(radv) * (g + n)  ==  radv * m * (Gr + N) / r^4

            float gr = RAG_G * dist;
            float r2 = dist * dist;
            float r4 = r2 * r2;

            this.acc = V2_Add(this.acc, V2_Mul(radv, that.mass * (gr + RAG_N) / r4));
        }
        arr[i].acc = this.acc;
    }

    #pragma omp parallel for firstprivate(arr, size, dt) default(none)
    for (int i = 0; i < size; i++) {
        Body *body = &arr[i];
        body->vel = V2_Add(body->vel, V2_Mul(body->acc, dt));
        body->pos = V2_Add(body->pos, V2_Mul(body->vel, dt));
    }
}

void World_GetBodies(World *w, Body **bodies, int *size) {
    SyncToArrFromGPU(w);
    *bodies = w->arr;
    *size = w->size;
}


void World_InitVK(World *w, const VulkanCtx *ctx) {
    if (w->comp == NULL) {
        WorldData data = (WorldData){
                .size = w->size,
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
