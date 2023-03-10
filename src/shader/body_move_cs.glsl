#version 450

#include "body_common_cs.glsl"

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= world.size) return;

    frame.bodies[i].vel += world.dt * frame.bodies[i].acc;
    frame.bodies[i].p.pos += world.dt * frame.bodies[i].vel;
    frame.bodies[i].acc = vec2(0);
}
