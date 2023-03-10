#include "body.h"

#include <stdlib.h>

/* Homegrown constants are the best. */
#define PI  3.14159274f

/* Convert radius to mass (R is evaluated 3 times). */
#define R_TO_M(R)   ((4.f * PI * DENSITY / 3.f) * (R) * (R) * (R))

/* Get random float between MIN and MAX. */
static float RangeRand(float min, float max) {
    return min + (max - min) * ((float)rand() / (float)RAND_MAX);
}

/* Randomize position, mass and radius of DENSITY. */
void Particle_InitRand(Particle *p, V2 min, V2 max) {
    float r = RangeRand(MIN_R, MAX_R);
    float x = RangeRand(min.x + r, max.x - r);
    float y = RangeRand(min.y + r, max.y - r);

    *p = (Particle){
            .pos = V2_From(x, y),
            .mass = R_TO_M(r),
            .radius = r,
    };
}

/* Apply gravitational pull of P to B. */
void Body_ApplyGrav(Body *b, Particle p) {
    V2 radv = V2_Sub(p.pos, b->p.pos);
    float dist = V2_Mag(radv);

    if (dist > b->p.radius + p.radius) {
        float g = G * p.mass / (dist * dist);
        // normalize(radv) * g  ==  (radv / dist) * g  ==  radv * (g / dist)
        b->acc = V2_Add(b->acc, V2_Mul(radv, g / dist));
    }
}

/* Apply T seconds of acceleration and velocity to BODY. */
void Body_Move(Body *body, float t) {
    V2 acc = V2_Add(body->acc, V2_Mul(body->vel, FRICTION));    // apply friction
    body->acc = V2_ZERO;                                        // reset acceleration

    body->vel = V2_Add(body->vel, V2_Mul(acc, t));              // apply acceleration
    body->p.pos = V2_Add(body->p.pos, V2_Mul(body->vel, t));    // apply velocity
}
