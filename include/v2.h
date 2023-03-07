#ifndef RAG_V2_H
#define RAG_V2_H

#include <x86/sse2.h>
#include <simde-math.h>

typedef union V2 {
    struct {
        double x;
        double y;
    };
    simde__m128d simd;
} V2;

#define V2_ZERO     (V2){ .x = 0.0, .y = 0.0 }
#define V2_of(X, Y) (V2){ .x = (X), .y = (Y) }

static inline V2 V2_add(V2 a, V2 b) {
    simde__m128d x = simde_mm_add_pd(a.simd, b.simd);
    return (V2) { .simd = x };
}

static inline V2 V2_sub(V2 a, V2 b) {
    simde__m128d x = simde_mm_sub_pd(a.simd, b.simd);
    return (V2) { .simd = x };
}

static inline V2 V2_scale(V2 a, double f) {
    simde__m128d b = simde_mm_set1_pd(f);
    simde__m128d x = simde_mm_mul_pd(a.simd, b);
    return (V2) { .simd = x };
}

static inline double V2_len(V2 v) {
    return simde_math_hypot(v.y, v.x);
}

#endif //RAG_V2_H
