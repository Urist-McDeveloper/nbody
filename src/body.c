#include "body.h"

#include <stdlib.h>

/*
 * Constants
 */

#define G       10.0f
#define MIN_R   2.0f
#define MAX_R   2.0f

#define PI  3.14159265358979323846f
#define C   1.0f
#define F   (4.0f * PI * C / 3.0f)

#define VELOCITY_DECAY  0.01f

/*
 * Utils
 */

static float RangeRand(float min, float max) {
    return min + (max - min) * (rand() / (float)RAND_MAX);
}

/*
 * Implementation
 */

void Particle_InitRand(Particle *p, V2 min, V2 max) {
    float radius = RangeRand(MIN_R, MAX_R);
    float mass = F * radius * radius * radius;
    float x = RangeRand(min.x + radius, max.x - radius);
    float y = RangeRand(min.y + radius, max.y - radius);

    *p = (Particle){
            .pos = V2_From(x, y),
            .mass = mass,
            .radius = radius,
    };
}

void Body_ApplyGrav(Body *b, Particle p) {
    V2 radv = V2_Sub(p.pos, b->p.pos);
    float dist = V2_Mag(radv);

    if (dist > b->p.radius + p.radius) {
        float g = G * p.mass / (dist * dist);
        // normalize(radv) * g  ==  (radv / dist) * g  ==  radv * (g / dist)
        b->acc = V2_Add(b->acc, V2_Mul(radv, g / dist));
    }
}

void Body_Move(Body *body, float t) {
    body->vel = V2_Add(body->vel, V2_Mul(body->acc, t));        // apply acceleration
    body->vel = V2_Mul(body->vel, 1.0f - VELOCITY_DECAY * t);   // apply decay
    body->p.pos = V2_Add(body->p.pos, V2_Mul(body->vel, t));    // apply velocity
    body->acc = V2_ZERO;                                        // reset acceleration
}
