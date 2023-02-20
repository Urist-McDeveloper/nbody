#ifndef V2_H
#define V2_H

#include <math.h>

typedef struct V2 {
    double x, y;
} V2;

#define V2_ZERO         (V2){ .x = 0.0, .y = 0.0 }
#define V2_From(X, Y)   (V2){ .x = (X), .y = (Y) }

static inline V2 V2_Neg(V2 a) {
    return V2_From(-a.x, -a.y);
}

static inline V2 V2_Add(V2 a, V2 b) {
    return V2_From(a.x + b.x, a.y + b.y);
}

static inline V2 V2_Sub(V2 a, V2 b) {
    return V2_From(a.x - b.x, a.y - b.y);
}

static inline double V2_Length(V2 a) {
    return hypot(a.x, a.y);
}

static inline V2 V2_Scale(V2 a, double f) {
    return V2_From(a.x * f, a.y * f);
}

static inline V2 V2_Normalize(V2 a) {
    double length = V2_Length(a);
    return length == 0.0 ? V2_ZERO : V2_Scale(a, 1.0 / length);
}

#endif