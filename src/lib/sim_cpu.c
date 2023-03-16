#include "sim_cpu.h"
#include "util.h"

#ifdef USE_AVX

#include <immintrin.h>

/* How much floats are packed together. */
#define PACK_SIZE   8

/* Create __m256 from FIELD of 8 P members. */
#define MMX_SET(P, FIELD) _mm256_set_ps(P[0].FIELD, P[1].FIELD, P[2].FIELD, P[3].FIELD, \
                                        P[4].FIELD, P[5].FIELD, P[6].FIELD, P[7].FIELD)

#define mx              __m256
#define mmx_set1_ps     _mm256_set1_ps
#define mmx_add_ps      _mm256_add_ps
#define mmx_sub_ps      _mm256_sub_ps
#define mmx_mul_ps      _mm256_mul_ps
#define mmx_div_ps      _mm256_div_ps
#define mmx_max_ps      _mm256_max_ps
#define mmx_sqrt_ps     _mm256_sqrt_ps
#define mmx_storeu_ps   _mm256_storeu_ps

#else

#include <xmmintrin.h>

/* How much floats are packed together. */
#define PACK_SIZE   4

/* Create __m128 from FIELD of 4 P members. */
#define MMX_SET(P, FIELD) _mm_set_ps(P[0].FIELD, P[1].FIELD, P[2].FIELD, P[3].FIELD)

#define mx              __m128
#define mmx_set1_ps     _mm_set1_ps
#define mmx_add_ps      _mm_add_ps
#define mmx_sub_ps      _mm_sub_ps
#define mmx_mul_ps      _mm_mul_ps
#define mmx_div_ps      _mm_div_ps
#define mmx_max_ps      _mm_max_ps
#define mmx_sqrt_ps     _mm_sqrt_ps
#define mmx_storeu_ps   _mm_storeu_ps

#endif

struct ParticlePack {
    mx x;   // position x
    mx y;   // position y
    mx m;   // mass
};

/* Merge PACK_SIZE particles into a single pack. */
static ParticlePack CreatePack(const Particle *p) {
    return (ParticlePack){
            .x = MMX_SET(p, pos.x),
            .y = MMX_SET(p, pos.y),
            .m = MMX_SET(p, mass),
    };
}

void AllocPackArray(ParticlePack **arr, uint32_t *len, uint32_t count) {
    if (count == 0) {
        *len = 0;
        *arr = NULL;
    } else {
        *len = count / PACK_SIZE + (count % PACK_SIZE == 0 ? 0 : 1);
        *arr = aligned_alloc(4 * PACK_SIZE, *len * sizeof(ParticlePack));
        ASSERT(*arr != NULL, "Failed to alloc %u ParticlePacks", *len);
    }
}

void PackParticles(uint32_t count, const Particle *ps, ParticlePack *packs) {
    uint32_t rem = count % PACK_SIZE;
    uint32_t n = count / PACK_SIZE;

    #pragma omp parallel for schedule(static, 10) firstprivate(ps, packs, n) default(none)
    for (uint32_t i = 0; i < n; i++) {
        packs[i] = CreatePack(&ps[i * PACK_SIZE]);
    }
    if (rem != 0) {
        Particle rest[PACK_SIZE];
        for (uint32_t i = 0; i < rem; i++) {
            rest[i] = ps[n * PACK_SIZE + i];
        }
        for (uint32_t i = PACK_SIZE; i > rem; i--) {
            rest[i - 1] = (Particle){0};
        }
        packs[n] = CreatePack(rest);
    }
}

/* Horizontal sun of X. Should probably be done with SIMD instructions. */
static float mmx_sum(mx x) {
    float f[PACK_SIZE], sum = 0;
    mmx_storeu_ps(f, x);

    for (int i = 0; i < PACK_SIZE; i++) {
        sum += f[i];
    }
    return sum;
}

void PackedUpdate(Particle *p, float dt, uint32_t packs_len, ParticlePack *packs) {
    const mx g = mmx_set1_ps(NB_G);         // gravitational constant
    const mx x = mmx_set1_ps(p->pos.x);     // position x
    const mx y = mmx_set1_ps(p->pos.y);     // position y
    const mx r = mmx_set1_ps(p->radius);    // radius

    mx ax = mmx_set1_ps(0.f);               // acceleration x
    mx ay = mmx_set1_ps(0.f);               // acceleration y

    for (uint32_t i = 0; i < packs_len; i++) {
        ParticlePack pack = packs[i];

        // delta x and delta y
        mx dx = mmx_sub_ps(pack.x, x);
        mx dy = mmx_sub_ps(pack.y, y);

        // distance squared
        mx dist_sq = mmx_add_ps(mmx_mul_ps(dx, dx),
                                mmx_mul_ps(dy, dy));

        mx r2 = mmx_add_ps(dist_sq, r); // distance^2, softened
        mx r1 = mmx_sqrt_ps(r2);        // distance^1, softened

        mx gm = mmx_mul_ps(pack.m, g);  // gravity times mass
        mx r3 = mmx_mul_ps(r1, r2);     // distance^3

        // acceleration == normalize(radv) * (Gm / dist^2)
        //              == (radv / dist) * (Gm / dist^2)
        //              == radv * (Gm / dist^3)
        mx f = mmx_div_ps(gm, r3);

        ax = mmx_add_ps(ax, mmx_mul_ps(dx, f));
        ay = mmx_add_ps(ay, mmx_mul_ps(dy, f));
    }

    p->acc = V2_FROM(mmx_sum(ax), mmx_sum(ay));
    p->vel = AddV2(p->vel, ScaleV2(p->acc, dt));
    p->pos = AddV2(p->pos, ScaleV2(p->vel, dt));
}
