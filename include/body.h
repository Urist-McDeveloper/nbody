#ifndef BODY_H
#define BODY_H

#include "v2.h"

typedef struct Body {
    V2 pos, vel, acc;
    double m, r;
} Body;

/* Set random X, Y and W of BODY. */
void Body_Init(Body *body, int mx, int my);

/* Add gravitational acceleration between A and B. */
void Body_ApplyGrav(Body *a, Body *b);

/* Apply T seconds of acceleration and velocity. */
void Body_Move(Body *body, double t);

#endif
