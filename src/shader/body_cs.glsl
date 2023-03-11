#version 450

struct Body {
    vec2 pos, vel, acc;
    float mass, radius;
};

layout (std140, binding = 0) uniform WorldData {
    uint size;
    float dt;
} world;

layout (std140, binding = 1) readonly buffer FrameOld {
    Body arr[];
} old;

layout (std140, binding = 2) buffer FrameNew {
    Body arr[];
} new;

/* Local group size as specialization constant. */
layout (local_size_x_id = 0) in;

/*
 * Gravitational constant; controls pulling force.
 *      g = RAG_G * mass / dist^2
 */
layout (constant_id = 1) const float G = 10;

/*
 * "Negative" gravity; controls pushing force.
 *      n = RAG_N * mass / dist^3
 */
layout (constant_id = 2) const float N = -1000;

/* A fraction of velocity that becomes friction. */
layout (constant_id = 3) const float FRICTION_F = -0.01;

/* Get acceleration enacted by B upon A. */
vec2 GetGrav(Body a, Body b) {
    if (a.pos == b.pos) return vec2(0);

    vec2 radv = b.pos - a.pos;
    float dist = max(length(radv), 0.5 * (a.radius + b.radius));

    //          g  ==  Gm / r^2
    //          n  ==  Nm / r^3
    // norm(radv)  ==  radv * (1 / r)
    //
    // norm(radv) * (g + n)  ==  radv * m * (Gr + N) / r^4

    float gr = G * dist;
    float r2 = dist * dist;
    float r4 = r2 * r2;

    return radv * (b.mass * (gr + N) / r4);
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= world.size) return;

    // initial acceleration is friction
    vec2 acc = FRICTION_F * old.arr[i].vel;

    for (uint j = 0; j < world.size; j++) {
        acc += GetGrav(old.arr[i], old.arr[j]);
    }

    vec2 vel = old.arr[i].vel + (world.dt * acc);
    vec2 pos = old.arr[i].pos + (world.dt * vel);

    new.arr[i].vel = vel;
    new.arr[i].pos = pos;

    // because old buffer could have been updated
    new.arr[i].mass = old.arr[i].mass;
    new.arr[i].radius = old.arr[i].radius;
}
