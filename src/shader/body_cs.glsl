#version 450

struct Particle {
    vec2 pos;
    float mass;
    float radius;
};

struct Body {
    Particle p;
    vec2 vel;
    vec2 acc;
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

/* Gravitational constant. */
layout (constant_id = 1) const float G = 10.0;

/* A fraction of velocity that becomes friction. */
layout (constant_id = 2) const float FRICTION_F = -0.01;

/* Get gravity acceleration of B upon A. */
vec2 GetGrav(Particle a, Particle b) {
    vec2 radv = b.pos - a.pos;
    float len = length(radv);

    if (length(radv) > a.radius + b.radius) {
        float g = G * b.mass / (len * len);
        // normalize(radv) * g  ==  (radv / dist) * g  ==  radv * (g / dist)
        return radv * (g / len);
    } else {
        return vec2(0, 0);
    }
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= world.size) return;

    // initial acceleration is friction
    vec2 acc = FRICTION_F * old.arr[i].vel;

    for (uint j = 0; j < world.size; j++) {
        acc += GetGrav(old.arr[i].p, old.arr[j].p);
    }

    vec2 vel = old.arr[i].vel + (world.dt * acc);
    vec2 pos = old.arr[i].p.pos + (world.dt * vel);

    new.arr[i].vel = vel;
    new.arr[i].p.pos = pos;

    // because old buffer could have been updated
    new.arr[i].p.mass = old.arr[i].p.mass;
    new.arr[i].p.radius = old.arr[i].p.radius;
}
