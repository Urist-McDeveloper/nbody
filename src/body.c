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

#define MIN_BM  1000.0
#define MAX_BM  9999.9

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

void Body_ApplyGrav(Body *a, Body *b) {
    V2 ab_radv = V2_Sub(b->pos, a->pos);
    V2 ab_norm = V2_Normalize(ab_radv);
    V2 ba_norm = V2_Neg(ab_norm);

    double dist = V2_Length(ab_radv);
    double mind = a->r + b->r;

    if (dist > mind) {
        double f = G / (dist * dist);
        a->acc = V2_Add(a->acc, V2_Scale(ab_norm, f * b->m));
        b->acc = V2_Add(b->acc, V2_Scale(ba_norm, f * a->m));
    } else {
        double f = 1.0 - (dist / mind);
        a->acc = V2_Add(a->acc, V2_Scale(ba_norm, f * b->m));
        b->acc = V2_Add(b->acc, V2_Scale(ab_norm, f * a->m));
    }
}

void Body_Move(Body *body, double t) {
    body->vel = V2_Add(body->vel, V2_Scale(body->acc, t));
    body->pos = V2_Add(body->pos, V2_Scale(body->vel, t));
    body->acc = V2_ZERO;
}
