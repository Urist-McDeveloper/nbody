#ifndef RAG_BODY_H
#define RAG_BODY_H

#include <rag.h>

/* Gravitational constant. */
#define G       10.0f

/* Minimum radius of a Particle. */
#define MIN_R   2.0f

/* Maximum radius of a Particle. */
#define MAX_R   2.0f

/* Density of a Particle (used to calculate mass from radius). */
#define DENSITY 1.0f

/* A fraction of velocity that becomes friction. */
#define FRICTION (-0.01f)

/* Randomize position, mass and radius of P. */
void Particle_InitRand(Particle *p, V2 min, V2 max);

/* Apply gravitational pull of P to B. */
void Body_ApplyGrav(Body *b, Particle p);

/* Apply T seconds of acceleration and velocity to BODY. */
void Body_Move(Body *body, float t);

#endif //RAG_BODY_H
