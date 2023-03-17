#include "sim_cpu.h"
#include "util.h"

#ifdef USE_AVX

#include <immintrin.h>

/* How many floats are packed together. */
#define SIMD_SIZE       8

/* Create simd_t from FIELD of 8 P members. */
#define SIMD_SET_ARR(P, FIELD)  _mm256_set_ps((P)[0].FIELD, (P)[1].FIELD, (P)[2].FIELD, (P)[3].FIELD, \
                                              (P)[4].FIELD, (P)[5].FIELD, (P)[6].FIELD, (P)[7].FIELD)

#define simd_t          __m256
#define simd_set1       _mm256_set1_ps
#define simd_setzero    _mm256_setzero_ps
#define simd_add        _mm256_add_ps
#define simd_sub        _mm256_sub_ps
#define simd_nul        _mm256_mul_ps
#define simd_div        _mm256_div_ps
#define simd_max        _mm256_max_ps
#define simd_sqrt       _mm256_sqrt_ps
#define simd_storeu     _mm256_storeu_ps

#else

#include <xmmintrin.h>

/* How many floats are packed together. */
#define SIMD_SIZE   4

/* Create simd_t from FIELD of 4 P members. */
#define SIMD_SET_ARR(P, FIELD)  _mm_set_ps((P)[0].FIELD, (P)[1].FIELD, (P)[2].FIELD, (P)[3].FIELD)

#define simd_t          __m128
#define simd_set1       _mm_set1_ps
#define simd_setzero    _mm_setzero_ps
#define simd_add        _mm_add_ps
#define simd_sub        _mm_sub_ps
#define simd_nul        _mm_mul_ps
#define simd_div        _mm_div_ps
#define simd_max        _mm_max_ps
#define simd_sqrt       _mm_sqrt_ps
#define simd_storeu     _mm_storeu_ps

#endif

struct ParticlePack {
    simd_t x;   // position x
    simd_t y;   // position y
    simd_t m;   // mass
};

/* Merge SIMD_SIZE particles into a single pack. */
static ParticlePack CreatePack(const Particle *p) {
    return (ParticlePack){
            .x = SIMD_SET_ARR(p, pos.x),
            .y = SIMD_SET_ARR(p, pos.y),
            .m = SIMD_SET_ARR(p, mass),
    };
}

void AllocPackArray(ParticlePack **arr, uint32_t *len, uint32_t count) {
    if (count == 0) {
        *len = 0;
        *arr = NULL;
    } else {
        *len = count / SIMD_SIZE + (count % SIMD_SIZE == 0 ? 0 : 1);
        *arr = aligned_alloc(4 * SIMD_SIZE, *len * sizeof(ParticlePack));
        ASSERT(*arr != NULL, "Failed to alloc %u ParticlePacks", *len);
    }
}

void PackParticles(uint32_t count, const Particle *ps, ParticlePack *packs) {
    uint32_t rem = count % SIMD_SIZE;
    uint32_t n = count / SIMD_SIZE;

    #pragma omp parallel for schedule(static, 10) firstprivate(ps, packs, n) default(none)
    for (uint32_t i = 0; i < n; i++) {
        packs[i] = CreatePack(&ps[i * SIMD_SIZE]);
    }
    if (rem != 0) {
        Particle rest[SIMD_SIZE];
        for (uint32_t i = 0; i < rem; i++) {
            rest[i] = ps[n * SIMD_SIZE + i];
        }
        for (uint32_t i = SIMD_SIZE; i > rem; i--) {
            rest[i - 1] = (Particle){0};
        }
        packs[n] = CreatePack(rest);
    }
}

/* Horizontal sun of X. Should probably be done with SIMD instructions. */
static float simd_sum(simd_t x) {
    float f[SIMD_SIZE], sum = 0;
    simd_storeu(f, x);

    for (int i = 0; i < SIMD_SIZE; i++) {
        sum += f[i];
    }
    return sum;
}

void PackedUpdate(Particle *p, float dt, uint32_t packs_len, ParticlePack *packs) {
    const simd_t g = simd_set1(NB_G);         // gravitational constant
    const simd_t x = simd_set1(p->pos.x);     // position x
    const simd_t y = simd_set1(p->pos.y);     // position y
    const simd_t r = simd_set1(p->radius);    // radius

    simd_t ax = simd_setzero();               // acceleration x
    simd_t ay = simd_setzero();               // acceleration y

    for (uint32_t i = 0; i < packs_len; i++) {
        ParticlePack pack = packs[i];

        // delta x and delta y
        simd_t dx = simd_sub(pack.x, x);
        simd_t dy = simd_sub(pack.y, y);

        // distance squared
        simd_t dist_sq = simd_add(simd_nul(dx, dx),
                                  simd_nul(dy, dy));

        simd_t r2 = simd_add(dist_sq, r); // distance^2, softened
        simd_t r1 = simd_sqrt(r2);        // distance^1, softened

        simd_t gm = simd_nul(pack.m, g);  // gravity times mass
        simd_t r3 = simd_nul(r1, r2);     // distance^3

        // acceleration == normalize(radv) * (Gm / dist^2)
        //              == (radv / dist) * (Gm / dist^2)
        //              == radv * (Gm / dist^3)
        simd_t f = simd_div(gm, r3);

        ax = simd_add(ax, simd_nul(dx, f));
        ay = simd_add(ay, simd_nul(dy, f));
    }

    p->acc = V2_FROM(simd_sum(ax), simd_sum(ay));
    p->vel = AddV2(p->vel, ScaleV2(p->acc, dt));
    p->pos = AddV2(p->pos, ScaleV2(p->vel, dt));
}
