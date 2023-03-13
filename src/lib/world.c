#include <rag.h>
#include <rag_vk.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <immintrin.h>

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

/* How much floats are packed together. */
#define PACK_SIZE   8

/* 8 particles packed into AVX registers. */
typedef struct ParticlePack {
    __m256 x;   // position x
    __m256 y;   // position y
    __m256 m;   // mass
    __m256 r;   // radius
} ParticlePack;

#define MM256_SET(P, FIELD) _mm256_set_ps(P[0].FIELD, P[1].FIELD, P[2].FIELD, P[3].FIELD,\
                                          P[4].FIELD, P[5].FIELD, P[6].FIELD, P[7].FIELD)

/* Pack 8 particles. */
static ParticlePack PackParticles(const Particle *p) {
    return (ParticlePack){
            .x = MM256_SET(p, pos.x),
            .y = MM256_SET(p, pos.y),
            .m = MM256_SET(p, mass),
            .r = MM256_SET(p, radius),
    };
}

/* Horizontal sun of X. Should probably be done with SIMD instructions. */
static float mm256_sum(__m256 x) {
    float f[PACK_SIZE], sum = 0;
    _mm256_storeu_ps(f, x);

    for (int i = 0; i < PACK_SIZE; i++) {
        sum += f[i];
    }
    return sum;
}

/* Update acceleration of P using N particle packs. */
static void PackedUpdateAcc(Particle *p, float dt, const int n, const ParticlePack *pack) {
    const __m256 m_half = _mm256_set1_ps(0.5f);     // packed 0.5f
    const __m256 m_g = _mm256_set1_ps(RAG_G);       // packed RAG_G
    const __m256 m_n = _mm256_set1_ps(RAG_N);       // packed RAG_N

    const __m256 m_x = _mm256_set1_ps(p->pos.x);    // position x
    const __m256 m_y = _mm256_set1_ps(p->pos.y);    // position y
    const __m256 m_r = _mm256_set1_ps(p->radius);   // radius

    __m256 m_ax = _mm256_set1_ps(0.f);              // acceleration x
    __m256 m_ay = _mm256_set1_ps(0.f);              // acceleration y

    for (int i = 0; i < n; i++) {
        // delta x, delta y and distance squared
        __m256 dx = _mm256_sub_ps(pack[i].x, m_x);
        __m256 dy = _mm256_sub_ps(pack[i].y, m_y);
        __m256 dist2 = _mm256_add_ps(_mm256_mul_ps(dx, dx), _mm256_mul_ps(dy, dy));

        // minimum distance == 0.5 * (radiusA + radiusB)
        __m256 min_r = _mm256_mul_ps(m_half, _mm256_add_ps(m_r, pack[i].r));
        dist2 = _mm256_max_ps(dist2, _mm256_mul_ps(min_r, min_r));

        __m256 dist1 = _mm256_sqrt_ps(dist2);       // distance
        __m256 dist4 = _mm256_mul_ps(dist2, dist2); // distance^4

        __m256 gd_n = _mm256_add_ps(_mm256_mul_ps(m_g, dist1), m_n);            // gd_n = G * dist + N
        __m256 total = _mm256_mul_ps(pack[i].m, _mm256_div_ps(gd_n, dist4));    // f = m * (G * dist + N) / dist^4

        m_ax = _mm256_add_ps(m_ax, _mm256_mul_ps(dx, total));
        m_ay = _mm256_add_ps(m_ay, _mm256_mul_ps(dy, total));
    }

    V2 acc = V2_From(mm256_sum(m_ax), mm256_sum(m_ay));
    V2 friction = V2_Mul(p->vel, RAG_FRICTION);

    p->acc = V2_Add(friction, acc);
    p->vel = V2_Add(p->vel, V2_Mul(p->acc, dt));
    p->pos = V2_Add(p->pos, V2_Mul(p->vel, dt));
}

struct World {
    Particle *arr;      // array of particles
    int arr_size;       // length of arr
    ParticlePack *pack; // array of packed particle data
    int pack_size;      // length of pack
    WorldComp *comp;    // Vulkan-related stuff
    bool gpu_sync;      // whether last change in GPU buffer is synced with the array
    bool arr_sync;      // whether last change in the array is synced with GPU buffer
};

static void UpdatePackedData(World *w) {
    const int rem = w->arr_size % PACK_SIZE;
    const int n = w->pack_size - (rem == 0 ? 0 : 1);

    Particle *arr = w->arr;
    ParticlePack *pack = w->pack;

    // pack full 8
    #pragma omp parallel for schedule(static, 10) firstprivate(arr, pack, n) default(none)
    for (int i = 0; i < n; i++) {
        pack[i] = PackParticles(&arr[i * PACK_SIZE]);
    }

    // pack remainder
    if (rem != 0) {
        Particle rest[PACK_SIZE];
        for (int i = 0; i < rem; i++) {
            rest[i] = w->arr[n * PACK_SIZE + i];
        }
        for (int i = PACK_SIZE; i > rem; i--) {
            rest[i - 1] = (Particle){0};
        }
        pack[n] = PackParticles(rest);
    }
}

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

World *World_Create(const int size, V2 min, V2 max) {
    World *world = ALLOC(1, World);
    ASSERT_MSG(world != NULL, "Failed to alloc World");

    Particle *arr = ALLOC(size, Particle);
    ASSERT_FMT(arr != NULL, "Failed to alloc %d Particle", size);

    const int rem = size % PACK_SIZE;
    const int pack_size = size / PACK_SIZE + (rem == 0 ? 0 : 1);

    ParticlePack *pack = ALLOC_ALIGNED(4 * PACK_SIZE, pack_size, ParticlePack);
    ASSERT_FMT(pack != NULL, "Failed to alloc %d ParticlePack", pack_size);

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

    *world = (World){
            .arr = arr,
            .arr_size = size,
            .pack = pack,
            .pack_size = pack_size,
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
        free(w->pack);
        free(w->arr);
        free(w);
    }
}

void World_Update(World *w, float dt, int n) {
    SyncToArrFromGPU(w);
    w->arr_sync = false;

    Particle *arr = w->arr;
    int arr_size = w->arr_size;
    ParticlePack *pack = w->pack;
    int pack_size = w->pack_size;

    for (int update_iter = 0; update_iter < n; update_iter++) {
        UpdatePackedData(w);

        #pragma omp parallel for schedule(static, 20) firstprivate(dt, arr, arr_size, pack, pack_size) default(none)
        for (int i = 0; i < arr_size; i++) {
            PackedUpdateAcc(&arr[i], dt, pack_size, pack);
        }
    }
}

void World_GetParticles(World *w, Particle **ps, int *size) {
    SyncToArrFromGPU(w);
    *ps = w->arr;
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
