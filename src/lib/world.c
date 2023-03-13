#include <rag.h>
#include <rag_vk.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <immintrin.h>

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

/* How much floats are packed together. */
#define PACK_SIZE   8

/* 8 bodies packed into SIMD registers. */
typedef struct PackedBody {
    __m256 x;   // position x
    __m256 y;   // position y
    __m256 m;   // mass
    __m256 r;   // radius
} PackedBody;

/* Pack 8 bodies. */
static PackedBody PackBodies(const Body *b) {
    return (PackedBody){
            .x = _mm256_set_ps(b[0].pos.x, b[1].pos.x, b[2].pos.x, b[3].pos.x,
                               b[4].pos.x, b[5].pos.x, b[6].pos.x, b[7].pos.x),
            .y = _mm256_set_ps(b[0].pos.y, b[1].pos.y, b[2].pos.y, b[3].pos.y,
                               b[4].pos.y, b[5].pos.y, b[6].pos.y, b[7].pos.y),
            .m = _mm256_set_ps(b[0].mass, b[1].mass, b[2].mass, b[3].mass,
                               b[4].mass, b[5].mass, b[6].mass, b[7].mass),
            .r = _mm256_set_ps(b[0].radius, b[1].radius, b[2].radius, b[3].radius,
                               b[4].radius, b[5].radius, b[6].radius, b[7].radius),
    };
}

/* Horizontal sun of X. Should probably be done with SIMD instructions. */
static float mm256_sum(__m256 x) {
    float f[8];
    _mm256_storeu_ps(f, x);

    float r = 0.f;
    for (int i = 0; i < 8; i++) {
        r += f[i];
    }
    return r;
}

/* Update acceleration of B using N packed bodies. */
static void PackedUpdateAcc(Body *b, const int n, const PackedBody *packed) {
    const __m256 half = _mm256_set1_ps(0.5f);       // packed 0.5f
    const __m256 packed_g = _mm256_set1_ps(RAG_G);  // packed RAG_G
    const __m256 packed_n = _mm256_set1_ps(RAG_N);  // packed RAG_N

    const __m256 bx = _mm256_set1_ps(b->pos.x);     // position x
    const __m256 by = _mm256_set1_ps(b->pos.y);     // position y
    const __m256 br = _mm256_set1_ps(b->radius);    // r

    __m256 ax = _mm256_set1_ps(0.f);                // acceleration x
    __m256 ay = _mm256_set1_ps(0.f);                // acceleration y

    for (int i = 0; i < n; i++) {
        // because writing p instead of p[i] is more convenient
        PackedBody p = packed[i];

        // delta x, delta y and distance squared
        __m256 dx = _mm256_sub_ps(p.x, bx);
        __m256 dy = _mm256_sub_ps(p.y, by);
        __m256 dist2 = _mm256_add_ps(_mm256_mul_ps(dx, dx), _mm256_mul_ps(dy, dy));

        // minimum distance == 0.5 * (radiusA + radiusB)
        __m256 min_r = _mm256_mul_ps(half, _mm256_add_ps(br, p.r));
        dist2 = _mm256_max_ps(dist2, _mm256_mul_ps(min_r, min_r));

        __m256 dist1 = _mm256_sqrt_ps(dist2);       // distance
        __m256 dist4 = _mm256_mul_ps(dist2, dist2); // distance^4

        __m256 gd = _mm256_mul_ps(packed_g, dist1);                     //   gd = G * dist
        __m256 gd_n = _mm256_add_ps(gd, packed_n);                      // gd_n = G * dist + N
        __m256 total = _mm256_mul_ps(p.m, _mm256_div_ps(gd_n, dist4));  //    f = m * (G * dist + N) / dist^4

        ax = _mm256_add_ps(ax, _mm256_mul_ps(dx, total));
        ay = _mm256_add_ps(ay, _mm256_mul_ps(dy, total));
    }

    V2 friction = V2_Mul(b->vel, RAG_FRICTION);
    V2 acc = V2_From(mm256_sum(ax), mm256_sum(ay));
    b->acc = V2_Add(friction, acc);
}

struct World {
    Body *arr;          // array of bodies
    int arr_size;       // length of arr
    PackedBody *pck;    // array of packed bodies' data
    int pck_size;       // length of pck
    WorldComp *comp;    // Vulkan-related stuff
    bool gpu_sync;      // whether last change in GPU buffer is synced with the array
    bool arr_sync;      // whether last change in the array is synced with GPU buffer
};

static void UpdatePackedData(World *w) {
    const int rem = w->arr_size % PACK_SIZE;
    const int n = w->pck_size - (rem == 0 ? 0 : 1);

    Body *arr = w->arr;
    PackedBody *pck = w->pck;

    // pack full 8
    #pragma omp parallel for schedule(static, 25) firstprivate(arr, pck, n) default(none)
    for (int i = 0; i < n; i++) {
        pck[i] = PackBodies(&arr[i * PACK_SIZE]);
    }

    // pack remainder
    if (rem != 0) {
        Body b[PACK_SIZE];
        for (int i = 0; i < rem; i++) {
            b[i] = w->arr[n * PACK_SIZE + i];
        }
        for (int i = PACK_SIZE; i > rem; i--) {
            b[i - 1] = (Body){0};
        }
        w->pck[n] = PackBodies(b);
    }
}

/* Copy data from GPU buffer to RAM if necessary. */
static void SyncToArrFromGPU(World *w) {
    if (!w->gpu_sync) {
        WorldComp_GetBodies(w->comp, w->arr);
        UpdatePackedData(w);
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

World *World_Create(const int size, V2 min, V2 max) {
    World *world = ALLOC(1, World);
    ASSERT_MSG(world != NULL, "Failed to alloc World");

    Body *arr = ALLOC(size, Body);
    ASSERT_FMT(arr != NULL, "Failed to alloc %d Body", size);

    const int rem = size % PACK_SIZE;
    const int pck_size = size / PACK_SIZE + (rem == 0 ? 0 : 1);

    PackedBody *pck = ALLOC_ALIGNED(4 * PACK_SIZE, pck_size, PackedBody);
    ASSERT_FMT(pck != NULL, "Failed to alloc %d PackedBody", pck_size);

    #pragma omp parallel for schedule(static, 100) firstprivate(arr, size, min, max) default(none)
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
            .arr_size = size,
            .pck = pck,
            .pck_size = pck_size,
            .comp = NULL,
            .gpu_sync = true,
            .arr_sync = true,
    };

    UpdatePackedData(world);
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
    int arr_size = w->arr_size;
    PackedBody *pck = w->pck;
    int pck_size = w->pck_size;

    #pragma omp parallel for schedule(static, 25) firstprivate(arr, arr_size, pck, pck_size) default(none)
    for (int i = 0; i < arr_size; i++) {
        PackedUpdateAcc(&arr[i], pck_size, pck);
    }

    #pragma omp parallel for schedule(static, 100) firstprivate(arr, arr_size, dt) default(none)
    for (int i = 0; i < arr_size; i++) {
        Body *body = &arr[i];
        body->vel = V2_Add(body->vel, V2_Mul(body->acc, dt));
        body->pos = V2_Add(body->pos, V2_Mul(body->vel, dt));
    }
    UpdatePackedData(w);
}

void World_GetBodies(World *w, Body **bodies, int *size) {
    SyncToArrFromGPU(w);
    *bodies = w->arr;
    *size = w->arr_size;
}

void World_InitVK(World *w, const VulkanCtx *ctx) {
    if (w->comp == NULL) {
        WorldData data = (WorldData){
                .size = w->arr_size,
                .dt = 0,
        };
        w->comp = WorldComp_Create(ctx, data);
        w->arr_sync = false;
    }
}

void World_UpdateVK(World *w, float dt, int n) {
    ASSERT_FMT(w->comp != NULL, "Vulkan has not been initialized for World %p", w);
    SyncFromArrToGPU(w);
    w->gpu_sync = false;
    WorldComp_DoUpdate(w->comp, dt, n);
}
