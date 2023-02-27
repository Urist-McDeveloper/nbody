#include "body.h"

#include <stdlib.h>
#include <math.h>

/*
 * Constants
 */

#define PI  3.14159265358979323846
#define C   1.0
#define F   (4.0 * PI * C / 3.0)

#define VELOCITY_DECAY  0.01

/*
 * Utils
 */

static double rangeRand(double min, double max) {
    return min + (max - min) * (1.0 * rand() / RAND_MAX);
}

/*
 * Implementation
 */

void Particle_init(Particle *p, V2 min, V2 max) {
    double mass = rangeRand(MIN_BM, MAX_BM);
    double radius = pow(mass / F, 1.0 / 3.0);
    double x = rangeRand(min.x + radius, max.x - radius);
    double y = rangeRand(min.y + radius, max.y - radius);

    *p = (Particle) {
            .pos = V2_from(x, y),
            .mass = mass,
            .radius = radius,
    };
}

void Body_applyGrav(Body *b, Particle p) {
    V2 radv = V2_sub(p.pos, b->p.pos);
    double dist = V2_length(radv);

    if (dist > b->p.radius + p.radius) {
        double g = G * p.mass / (dist * dist);
        // normalize(radv) * g  ==  (radv / dist) * g  ==  radv * (g / dist)
        b->acc = V2_add(b->acc, V2_scale(radv, g / dist));
    }
}

void Body_move(Body *body, double t) {
    body->vel = V2_add(body->vel, V2_scale(body->acc, t));
    body->vel = V2_scale(body->vel, 1.0 - VELOCITY_DECAY * t);

    body->p.pos = V2_add(body->p.pos, V2_scale(body->vel, t));
    body->acc = V2_ZERO;
}
