#include "body.h"

#include <stdlib.h>

static float RangeRand(float min, float max) {
    return min + (max - min) * ((float)rand() / (float)RAND_MAX);
}

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
    V2 acc = V2_Add(body->acc, V2_Mul(body->vel, FRICTION));    // apply friction
    body->acc = V2_ZERO;                                        // reset acceleration

    body->vel = V2_Add(body->vel, V2_Mul(acc, t));              // apply acceleration
    body->p.pos = V2_Add(body->p.pos, V2_Mul(body->vel, t));    // apply velocity
}
