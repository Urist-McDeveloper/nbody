#ifndef RAG_BODY_H
#define RAG_BODY_H

#include "v2.h"

#define G       22.2
#define MIN_BM  33.3
#define MAX_BM  99.9

/*
 * PARTICLE
 */

typedef struct Particle {
    V2 pos;
    double mass;
    double radius;
} Particle;

/*
 * BODY
 */

typedef struct Body {
    Particle p;
    V2 vel, acc;
} Body;

/* Randomize position and mass of P. */
void Particle_init(Particle *p, V2 min, V2 max);

/* Apply gravitational pull of P to B. */
void Body_applyGrav(Body *b, Particle p);

/* Apply T seconds of acceleration and velocity to BODY. */
void Body_move(Body *body, double t);

#endif //RAG_BODY_H
