#ifndef RAG_V2_H
#define RAG_V2_H

#include <math.h>

typedef struct V2 {
    double x, y;
} V2;

#define V2_ZERO         (V2){ .x = 0.0, .y = 0.0 }
#define V2_From(X, Y)   (V2){ .x = (X), .y = (Y) }

static inline V2 V2_add(V2 a, V2 b) {
    return V2_From(a.x + b.x, a.y + b.y);
}

static inline V2 V2_sub(V2 a, V2 b) {
    return V2_From(a.x - b.x, a.y - b.y);
}

static inline double V2_length(V2 a) {
    return hypot(a.x, a.y);
}

static inline V2 V2_scale(V2 a, double f) {
    return V2_From(a.x * f, a.y * f);
}

#endif //RAG_V2_H
