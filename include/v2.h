#ifndef RAG_V2_H
#define RAG_V2_H

#include <x86/sse2.h>
#include <math.h>

typedef union V2 {
    struct {
        double x;
        double y;
    };
    simde__m128d simd;
} V2;

/* Zero-length vector. */
#define V2_ZERO         (V2){ .x = 0.0, .y = 0.0 }

/* Constructor macro. */
#define V2_From(X, Y)   (V2){ .x = (X), .y = (Y) }

/* Vector addition. */
#define V2_Add(A, B)    (V2){ .simd = simde_mm_add_pd((A).simd, (B).simd) }

/* Vector subtraction. */
#define V2_Sub(A, B)    (V2){ .simd = simde_mm_sub_pd((A).simd, (B).simd) }

/* Scalar multiplication. */
#define V2_Mul(V, F)    (V2){ .simd = simde_mm_mul_pd((V).simd, simde_mm_set1_pd(F)) }

/* Vector magnitude. */
static inline double V2_Mag(V2 v) {
    return hypot(v.x, v.y);
}

/* Vector magnitude squared. */
static inline double V2_SqMag(V2 v) {
    return (v.x * v.x) + (v.y * v.y);
}

#endif //RAG_V2_H
