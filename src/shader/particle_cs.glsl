#version 450

struct Particle {
    vec2 pos, vel, acc;
    float mass, radius;
};

layout (std140, binding = 0) uniform WorldData {
    uint size;
    float dt;
} world;

layout (std140, binding = 1) readonly buffer FrameOld {
    Particle arr[];
} old;

layout (std140, binding = 2) buffer FrameNew {
    Particle arr[];
} new;

/* Local group size as specialization constant. */
layout (local_size_x_id = 0) in;

/*
 * Gravitational constant; gravity is proportional to the inverse square of distance.
 *      g = G * mass / dist^2
 */
layout (constant_id = 1) const float G = 10;

/*
 * Repulsion constant; repulsion is proportional to the inverse cube of distance.
 *      n = N * mass / dist^3
 */
layout (constant_id = 2) const float N = -1000;

/*
 * A fraction of velocity that becomes deceleration.
 *      f = F * velocity
 */
layout (constant_id = 3) const float F = -0.01;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= world.size) return;

    Particle a = old.arr[i];
    vec2 acc = F * old.arr[i].vel;

    for (uint j = 0; j < world.size; j++) {
        Particle b = old.arr[j];

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

        acc += radv * (b.mass * (gr + N) / r4);
    }

    vec2 vel = old.arr[i].vel + (world.dt * acc);
    vec2 pos = old.arr[i].pos + (world.dt * vel);

    new.arr[i].acc = acc;
    new.arr[i].vel = vel;
    new.arr[i].pos = pos;

    // because old buffer could have been updated
    new.arr[i].mass = old.arr[i].mass;
    new.arr[i].radius = old.arr[i].radius;
}
