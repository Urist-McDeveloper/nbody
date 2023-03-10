#version 450

#include "body_common_cs.glsl"

/* A fraction of velocity that becomes friction. */
const float FRICTION_F = -0.01;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= world.size) return;

    // acceleration from gravity + friction
    vec2 friction = FRICTION_F * frame.bodies[i].vel;
    vec2 final_acc = frame.bodies[i].acc + friction;

    frame.bodies[i].acc = vec2(0);
    frame.bodies[i].vel += world.dt * final_acc;
    frame.bodies[i].p.pos += world.dt * frame.bodies[i].vel;
}
