#include "particle_pack.h"
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
    mx r;   // radius
};

/* Merge PACK_SIZE particles into a single pack. */
static ParticlePack CreatePack(const Particle *p) {
    return (ParticlePack){
            .x = MMX_SET(p, pos.x),
            .y = MMX_SET(p, pos.y),
            .m = MMX_SET(p, mass),
            .r = MMX_SET(p, radius),
    };
}

void AllocPackArray(uint32_t count, ParticlePack **arr, uint32_t *len) {
    *len = count / PACK_SIZE + (count % PACK_SIZE == 0 ? 0 : 1);
    *arr = ALLOC_ALIGNED(4 * PACK_SIZE, *len, ParticlePack);
    ASSERT_FMT(*arr != NULL, "Failed to alloc %u ParticlePacks", *len);
}

void PackParticles(uint32_t count, const Particle *ps, ParticlePack *packs) {
    uint32_t rem = count % PACK_SIZE;
    uint32_t n = count / PACK_SIZE - (rem == 0 ? 0 : 1);

    #pragma omp parallel for schedule(static, 10) firstprivate(ps, packs, n) default(none)
    for (int i = 0; i < n; i++) {
        packs[i] = CreatePack(&ps[i * PACK_SIZE]);
    }
    if (rem != 0) {
        Particle rest[PACK_SIZE];
        for (int i = 0; i < rem; i++) {
            rest[i] = ps[n * PACK_SIZE + i];
        }
        for (int i = PACK_SIZE; i > rem; i--) {
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
    const mx m_half = mmx_set1_ps(0.5f);    // 0.5f
    const mx m_g = mmx_set1_ps(NB_G);       // gravitational constant
    const mx m_n = mmx_set1_ps(NB_N);       // repulsion constant

    const mx m_x = mmx_set1_ps(p->pos.x);   // position x
    const mx m_y = mmx_set1_ps(p->pos.y);   // position y
    const mx m_r = mmx_set1_ps(p->radius);  // radius

    mx m_ax = mmx_set1_ps(0.f);             // acceleration x
    mx m_ay = mmx_set1_ps(0.f);             // acceleration y

    for (int i = 0; i < packs_len; i++) {
        // delta x, delta y and distance squared
        mx dx = mmx_sub_ps(packs[i].x, m_x);
        mx dy = mmx_sub_ps(packs[i].y, m_y);
        mx dist2 = mmx_add_ps(mmx_mul_ps(dx, dx), mmx_mul_ps(dy, dy));

        // minimum distance == 0.5 * (radiusA + radiusB)
        mx min_r = mmx_mul_ps(m_half, mmx_add_ps(m_r, packs[i].r));
        dist2 = mmx_max_ps(dist2, mmx_mul_ps(min_r, min_r));

        mx dist1 = mmx_sqrt_ps(dist2);       // distance
        mx dist4 = mmx_mul_ps(dist2, dist2); // distance^4

        mx gd_n = mmx_add_ps(mmx_mul_ps(m_g, dist1), m_n);          // gd_n = G * dist + N
        mx res = mmx_mul_ps(packs[i].m, mmx_div_ps(gd_n, dist4));   // res = m * (G * dist + N) / dist^4

        m_ax = mmx_add_ps(m_ax, mmx_mul_ps(dx, res));
        m_ay = mmx_add_ps(m_ay, mmx_mul_ps(dy, res));
    }

    V2 acc = V2_FROM(mmx_sum(m_ax), mmx_sum(m_ay));
    V2 friction = ScaleV2(p->vel, NB_F);

    p->acc = AddV2(friction, acc);
    p->vel = AddV2(p->vel, ScaleV2(p->acc, dt));
    p->pos = AddV2(p->pos, ScaleV2(p->vel, dt));
}
