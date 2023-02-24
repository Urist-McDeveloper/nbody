#ifndef RAG_BODY_H
#define RAG_BODY_H

#include "v2.h"

#define G       100.0
#define MIN_BM  50.0
#define MAX_BM  50.0

typedef struct Body {
    V2 pos, vel, acc;
    double m, r;
} Body;

/* Set random X, Y and W of BODY. */
void Body_init(Body *body, int mx, int my);

/* Apply gravitational pull of OTHER to TARGET. */
void Body_applyGrav(Body *target, Body *other);

/* Apply T seconds of acceleration and velocity to BODY. */
void Body_move(Body *body, double t);

#endif //RAG_BODY_H
