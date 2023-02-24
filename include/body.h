#ifndef BODY_H
#define BODY_H

#include "v2.h"

#define G       10.0
#define MIN_BM  100.0
#define MAX_BM  999.9

typedef struct Body {
    V2 pos, vel, acc;
    double m, r;
} Body;

/* Set random X, Y and W of BODY. */
void Body_init(Body *body, int mx, int my);

/* Apply gravitational pull of OTHER to TARGET. */
void Body_applyGrav(Body *target, Body *other);
void Body_applyGravV2(Body *target, V2 other, double mass);

/* Apply T seconds of acceleration and velocity to BODY. */
void Body_move(Body *body, double t);

#endif
