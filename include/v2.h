#ifndef RAG_V2_H
#define RAG_V2_H

#include <math.h>

/* 2D vector of floats. */
typedef struct V2 {
    float x, y;
} V2;

/* Zero-length vector. */
#define V2_ZERO         (V2){ .x = 0.0, .y = 0.0 }

/* Constructor macro. */
#define V2_From(X, Y)   (V2){ .x = (X), .y = (Y) }

/* Vector addition. */
static inline V2 V2_Add(V2 a, V2 b) {
    return V2_From(a.x + b.x, a.y + b.y);
}

/* Vector subtraction. */
static inline V2 V2_Sub(V2 a, V2 b) {
    return V2_From(a.x - b.x, a.y - b.y);
}

/* Scalar multiplication. */
static inline V2 V2_Mul(V2 v, float f) {
    return V2_From(v.x * f, v.y * f);
}

/* Vector magnitude. */
static inline float V2_Mag(V2 v) {
    return hypotf(v.x, v.y);
}

/* Vector magnitude squared. */
static inline float V2_SqMag(V2 v) {
    return (v.x * v.x) + (v.y * v.y);
}

#endif //RAG_V2_H
