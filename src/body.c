#include "body.h"

#include <stdlib.h>
#include <math.h>

/*
 * Constants
 */

#define PI  3.14159265358979323846
#define C   1.0
#define F   (4.0 * PI * C / 3.0)

#define G   10.0

#define MIN_BM  10.0
#define MAX_BM  99.9

/*
 * Utils
 */

static double range_rand(double min, double max) {
    return min + (max - min) * (1.0 * rand() / RAND_MAX);
}

/*
 * Implementation
 */

void Body_Init(Body *body, int mx, int my) {
    double mass = range_rand(MIN_BM, MAX_BM);
    double radius = pow(mass / F, 1.0 / 3.0);
    double x = range_rand(radius, mx - radius);
    double y = range_rand(radius, my - radius);

    *body = (Body) {
            .pos = V2_From(x, y),
            .vel = V2_ZERO,
            .acc = V2_ZERO,
            .m = mass,
            .r = radius,
    };
}

/* Apply gravitational pull of OTHER to TARGET. */
void Body_ApplyGravUni(Body *target, Body *other) {
    V2 radv = V2_Sub(other->pos, target->pos);
    double dist = V2_Length(radv);
    double dmin = target->r + other->r;

    if (dist > dmin) {
        double g = G * other->m / (dist * dist);
        // normalize(radv) * g  ==  (radv / dist) * g  ==  radv * (g / dist)
        target->acc = V2_Add(target->acc, V2_Scale(radv, g / dist));
    } else {
        double f = 1.0 - (dist / dmin);
        target->acc = V2_Add(target->acc, V2_Scale(radv, -f * other->m));
    }
}

void Body_Move(Body *body, double t) {
    body->vel = V2_Add(body->vel, V2_Scale(body->acc, t));
    body->pos = V2_Add(body->pos, V2_Scale(body->vel, t));
    body->acc = V2_ZERO;
}
