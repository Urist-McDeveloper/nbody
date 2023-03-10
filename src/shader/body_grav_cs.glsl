#version 450

#include "body_common_cs.glsl"

/* Gravitational constant. */
const float G = 10.0;

/* Get gravity acceleration of P upon B. */
vec2 GetGrav(Body b, Particle p) {
    vec2 radv = p.pos - b.p.pos;
    float len = length(radv);

    if (length(radv) > b.p.radius + p.radius) {
        float g = G * p.mass / (len * len);
        // normalize(radv) * g  ==  (radv / dist) * g  ==  radv * (g / dist)
        return radv * (g / len);
    } else {
        return vec2(0, 0);
    }
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= world.size) return;

    for (uint j = 0; j < world.size; j++) {
        frame.bodies[i].acc += GetGrav(frame.bodies[i], frame.bodies[j].p);
    }
}
