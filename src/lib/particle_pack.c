#include "particle_pack.h"
#include "util.h"

#include <stdint.h>
#include <immintrin.h>

/* How much floats are packed together. */
#define PACK_SIZE   8

struct ParticlePack {
    __m256 x;   // position x
    __m256 y;   // position y
    __m256 m;   // mass
    __m256 r;   // radius
};

/* Horizontal sun of X. Should probably be done with SIMD instructions. */
static float mm256_sum(__m256 x) {
    float f[PACK_SIZE], sum = 0;
    _mm256_storeu_ps(f, x);

    for (int i = 0; i < PACK_SIZE; i++) {
        sum += f[i];
    }
    return sum;
}

#define MM256_SET(P, FIELD) _mm256_set_ps(P[0].FIELD, P[1].FIELD, P[2].FIELD, P[3].FIELD,\
                                          P[4].FIELD, P[5].FIELD, P[6].FIELD, P[7].FIELD)

/* Pack 8 particles. */
static ParticlePack CreatePack(const Particle *p) {
    return (ParticlePack){
            .x = MM256_SET(p, pos.x),
            .y = MM256_SET(p, pos.y),
            .m = MM256_SET(p, mass),
            .r = MM256_SET(p, radius),
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

void PackedUpdate(Particle *p, float dt, size_t packs_len, ParticlePack *packs) {
    const __m256 m_half = _mm256_set1_ps(0.5f);     // packed 0.5f
    const __m256 m_g = _mm256_set1_ps(RAG_G);       // packed RAG_G
    const __m256 m_n = _mm256_set1_ps(RAG_N);       // packed RAG_N

    const __m256 m_x = _mm256_set1_ps(p->pos.x);    // position x
    const __m256 m_y = _mm256_set1_ps(p->pos.y);    // position y
    const __m256 m_r = _mm256_set1_ps(p->radius);   // radius

    __m256 m_ax = _mm256_set1_ps(0.f);              // acceleration x
    __m256 m_ay = _mm256_set1_ps(0.f);              // acceleration y

    for (int i = 0; i < packs_len; i++) {
        // delta x, delta y and distance squared
        __m256 dx = _mm256_sub_ps(packs[i].x, m_x);
        __m256 dy = _mm256_sub_ps(packs[i].y, m_y);
        __m256 dist2 = _mm256_add_ps(_mm256_mul_ps(dx, dx), _mm256_mul_ps(dy, dy));

        // minimum distance == 0.5 * (radiusA + radiusB)
        __m256 min_r = _mm256_mul_ps(m_half, _mm256_add_ps(m_r, packs[i].r));
        dist2 = _mm256_max_ps(dist2, _mm256_mul_ps(min_r, min_r));

        __m256 dist1 = _mm256_sqrt_ps(dist2);       // distance
        __m256 dist4 = _mm256_mul_ps(dist2, dist2); // distance^4

        __m256 gd_n = _mm256_add_ps(_mm256_mul_ps(m_g, dist1), m_n);            // gd_n = G * dist + N
        __m256 total = _mm256_mul_ps(packs[i].m, _mm256_div_ps(gd_n, dist4));   // f = m * (G * dist + N) / dist^4

        m_ax = _mm256_add_ps(m_ax, _mm256_mul_ps(dx, total));
        m_ay = _mm256_add_ps(m_ay, _mm256_mul_ps(dy, total));
    }

    V2 acc = V2_From(mm256_sum(m_ax), mm256_sum(m_ay));
    V2 friction = V2_Mul(p->vel, RAG_FRICTION);

    p->acc = V2_Add(friction, acc);
    p->vel = V2_Add(p->vel, V2_Mul(p->acc, dt));
    p->pos = V2_Add(p->pos, V2_Mul(p->vel, dt));
}
