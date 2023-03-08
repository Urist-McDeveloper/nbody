#include "body.h"

#include <stdlib.h>

/*
 * Constants
 */

#define G       10.0
#define MIN_R   2.0
#define MAX_R   2.0

#define PI  3.14159265358979323846
#define C   1.0
#define F   (4.0 * PI * C / 3.0)

#define VELOCITY_DECAY  0.01

/*
 * Utils
 */

static double RangeRand(double min, double max) {
    return min + (max - min) * (1.0 * rand() / RAND_MAX);
}

/*
 * Implementation
 */

void Particle_InitRand(Particle *p, V2 min, V2 max) {
    double radius = RangeRand(MIN_R, MAX_R);
    double mass = F * radius * radius * radius;
    double x = RangeRand(min.x + radius, max.x - radius);
    double y = RangeRand(min.y + radius, max.y - radius);

    *p = (Particle) {
            .pos = V2_From(x, y),
            .mass = mass,
            .radius = radius,
    };
}

void Body_ApplyGrav(Body *b, Particle p) {
    V2 radv = V2_Sub(p.pos, b->p.pos);
    double dist = V2_Mag(radv);

    if (dist > b->p.radius + p.radius) {
        double g = G * p.mass / (dist * dist);
        // normalize(radv) * g  ==  (radv / dist) * g  ==  radv * (g / dist)
        b->acc = V2_Add(b->acc, V2_Mul(radv, g / dist));
    }
}

void Body_Move(Body *body, double t) {
    body->vel = V2_Add(body->vel, V2_Mul(body->acc, t));        // apply acceleration
    body->vel = V2_Mul(body->vel, 1.0 - VELOCITY_DECAY * t);    // apply decay
    body->p.pos = V2_Add(body->p.pos, V2_Mul(body->vel, t));    // apply velocity
    body->acc = V2_ZERO;                                        // reset acceleration
}
