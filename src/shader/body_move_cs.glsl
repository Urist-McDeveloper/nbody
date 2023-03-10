#version 450

#include "body_common_cs.glsl"

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= ubo.size) return;

    sbo.bodies[i].vel += ubo.dt * sbo.bodies[i].acc;
    sbo.bodies[i].p.pos += ubo.dt * sbo.bodies[i].vel;
    sbo.bodies[i].acc = vec2(0);
}
