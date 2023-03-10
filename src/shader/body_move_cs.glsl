#version 450

#include "body_common_cs.glsl"

/* How velocity changes along the axis of bounce. */
const float BOUNCE_ALONG = -0.5f;

/* How velocity changes along the other axis. */
const float BOUNCE_OPPOSITE = 0.75f;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= world.size) return;

    frame.bodies[i].vel += world.dt * frame.bodies[i].acc;
    frame.bodies[i].p.pos += world.dt * frame.bodies[i].vel;
    frame.bodies[i].acc = vec2(0);

    if (frame.bodies[i].p.pos.x < world.min.x) {
        frame.bodies[i].p.pos.x = world.min.x;
        frame.bodies[i].vel.x *= BOUNCE_ALONG;
        frame.bodies[i].vel.y *= BOUNCE_OPPOSITE;
    }
    if (frame.bodies[i].p.pos.x > world.max.x) {
        frame.bodies[i].p.pos.x = world.max.x;
        frame.bodies[i].vel.x *= BOUNCE_ALONG;
        frame.bodies[i].vel.y *= BOUNCE_OPPOSITE;
    }
    if (frame.bodies[i].p.pos.y < world.min.y) {
        frame.bodies[i].p.pos.y = world.min.y;
        frame.bodies[i].vel.y *= BOUNCE_ALONG;
        frame.bodies[i].vel.x *= BOUNCE_OPPOSITE;
    }
    if (frame.bodies[i].p.pos.y > world.max.y) {
        frame.bodies[i].p.pos.y = world.max.y;
        frame.bodies[i].vel.y *= BOUNCE_ALONG;
        frame.bodies[i].vel.x *= BOUNCE_OPPOSITE;
    }
}
