#ifndef RAG_BODY_H
#define RAG_BODY_H

#include "v2.h"

#define G       22.2
#define MIN_BM  33.3
#define MAX_BM  99.9

typedef struct Body {
    V2 pos, vel, acc;
    double m, r;
} Body;

/* Set random X, Y and W of BODY. */
void Body_init(Body *body, int mx, int my);

/* Apply gravitational pull of OTHER to TARGET. */
void Body_applyGrav(Body *target, const Body *other);

void Body_applyGravV2(Body *target, V2 com, double mass);

/* Apply T seconds of acceleration and velocity to BODY. */
void Body_move(Body *body, double t);

#endif //RAG_BODY_H
