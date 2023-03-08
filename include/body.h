#ifndef RAG_BODY_H
#define RAG_BODY_H

#include "v2.h"

/*
 * PARTICLE
 */

typedef struct Particle {
    V2 pos;
    float mass;
    float radius;
} Particle;

/*
 * BODY
 */

typedef struct Body {
    Particle p;
    V2 vel, acc;
} Body;

/* Randomize position, mass and radius of P. */
void Particle_InitRand(Particle *p, V2 min, V2 max);

/* Apply gravitational pull of P to B. */
void Body_ApplyGrav(Body *b, Particle p);

/* Apply T seconds of acceleration and velocity to BODY. */
void Body_Move(Body *body, float t);

#endif //RAG_BODY_H
