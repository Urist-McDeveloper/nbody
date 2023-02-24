#include "body.h"

#include <stdlib.h>
#include <math.h>

/*
 * Constants
 */

#define PI  3.14159265358979323846
#define C   1.0
#define F   (4.0 * PI * C / 3.0)

/*
 * Utils
 */

static double rangeRand(double min, double max) {
    return min + (max - min) * (1.0 * rand() / RAND_MAX);
}

/*
 * Implementation
 */

void Body_init(Body *body, int mx, int my) {
    double mass = rangeRand(MIN_BM, MAX_BM);
    double radius = pow(mass / F, 1.0 / 3.0);
    double x = rangeRand(radius, mx - radius);
    double y = rangeRand(radius, my - radius);

    *body = (Body) {
            .pos = V2_From(x, y),
            .vel = V2_ZERO,
            .acc = V2_ZERO,
            .m = mass,
            .r = radius,
    };
}

static void applyGrav(Body *target, V2 radv, double dist, double mass) {
    double g = G * mass / (dist * dist);
    // normalize(radv) * g  ==  (radv / dist) * g  ==  radv * (g / dist)
    target->acc = V2_add(target->acc, V2_scale(radv, g / dist));
}

void Body_applyGrav(Body *target, Body *other) {
    V2 radv = V2_sub(other->pos, target->pos);
    double dist = V2_length(radv);

    if (dist > target->r + other->r) {
        applyGrav(target, radv, dist, other->m);
    }
}

void Body_move(Body *body, double t) {
    body->vel = V2_add(body->vel, V2_scale(body->acc, t));
    body->pos = V2_add(body->pos, V2_scale(body->vel, t));
    body->acc = V2_ZERO;
}
