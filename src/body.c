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
            .pos = V2_from(x, y),
            .vel = V2_ZERO,
            .acc = V2_ZERO,
            .m = mass,
            .r = radius,
    };
}

static void applyGrav(Body *target, double mass, V2 radv, double dist) {
    double g = G * mass / (dist * dist);
    // normalize(radv) * g  ==  (radv / dist) * g  ==  radv * (g / dist)
    target->acc = V2_add(target->acc, V2_scale(radv, g / dist));
}

void Body_applyGrav(Body *target, const Body *other) {
    if (target == other) return;

    V2 radv = V2_sub(other->pos, target->pos);
    double dist = V2_length(radv);

    if (dist > target->r + other->r) {
        applyGrav(target, other->m, radv, dist);
    } else {
        // TODO: impulse-based collision logic
//        V2 tgt_p = V2_scale(target->vel, target->m);
//        V2 oth_p = V2_scale(other->vel, other->m);
    }
}

void Body_applyGravV2(Body *target, V2 com, double mass) {
    V2 radv = V2_sub(com, target->pos);
    double dist = V2_length(radv);
    applyGrav(target, mass, com, dist);
}

void Body_move(Body *body, double t) {
    body->vel = V2_add(body->vel, V2_scale(body->acc, t));
    body->pos = V2_add(body->pos, V2_scale(body->vel, t));
    body->acc = V2_ZERO;
}
