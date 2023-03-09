#ifndef RAG_BODY_H
#define RAG_BODY_H

#include <rag.h>

#define G       10.0f
#define MIN_R   2.0f
#define MAX_R   2.0f

#define PI  3.14159265358979323846f
#define C   1.0f
#define F   (4.0f * PI * C / 3.0f)

#define VELOCITY_DECAY  0.01f

/* Randomize position, mass and radius of P. */
void Particle_InitRand(Particle *p, V2 min, V2 max);

/* Apply gravitational pull of P to B. */
void Body_ApplyGrav(Body *b, Particle p);

/* Apply T seconds of acceleration and velocity to BODY. */
void Body_Move(Body *body, float t);

#endif //RAG_BODY_H
