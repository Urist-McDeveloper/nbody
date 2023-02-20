#ifndef BODY_H
#define BODY_H

#include "v2.h"

typedef struct Body {
    V2 pos, vel, acc;
    double m, r;
} Body;

/* Set random X, Y and W of BODY. */
void Body_Init(Body *body, int mx, int my);

/* Apply gravitational pull of both A and B to each other. */
void Body_ApplyGravBoth(Body *a, Body *b);

/* Apply gravitational pull of OTHER to TARGET. */
void Body_ApplyGravUni(Body *target, Body *other);

/* Apply T seconds of acceleration and velocity to BODY. */
void Body_Move(Body *body, double t);

#endif
