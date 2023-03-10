#version 450

#include "body_common_cs.glsl"

const float G = 10.0;

/* Get gravity acceleration of P upon B. */
vec2 GetGrav(Body b, Particle p) {
    vec2 radv = b.p.pos - p.pos;
    float len = length(radv);

    if (length(radv) > b.p.radius + p.radius) {
        float g = G * p.mass / (len * len);
        return radv / (g * len);
    } else {
        return vec2(0, 0);
    }
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= ubo.size) return;

    sbo.bodies[i].vel += ubo.dt * sbo.bodies[i].acc;
    sbo.bodies[i].p.pos += ubo.dt * sbo.bodies[i].vel;
    sbo.bodies[i].acc = vec2(0);
}
