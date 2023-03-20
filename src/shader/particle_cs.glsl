#version 450

struct Particle {
    vec2 pos, vel, acc;
    float mass, radius;
};

layout (std140, binding = 0) uniform WorldData {
    uint total_len; // total number of particles
    uint mass_len;  // number of particles with mass
    float dt;       // time delta
} world;

layout (std140, binding = 1) readonly buffer FrameOld {
    Particle arr[];
} old;

layout (std140, binding = 2) buffer FrameNew {
    Particle arr[];
} new;

/* Local group size as specialization constant. */
layout (local_size_x_id = 0) in;

/* Gravitational constant; `g = NB_G * mass / dist^2`. */
layout (constant_id = 1) const float G = 0;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= world.total_len) return;

    Particle p = old.arr[i];
    p.acc = vec2(0);

    for (uint j = 0; j < world.mass_len; j++) {
        Particle other = old.arr[j];

        vec2 radv = other.pos - p.pos;      // radius-vector
        float dist_sq = dot(radv, radv);    // distance^2

        float r2 = dist_sq + p.radius;      // distance^2, softened
        float r1 = sqrt(r2);                // distance^2, softened
        float r3 = r1 * r2;                 // distance^3, softened

        // acceleration == normalize(radv) * (Gm / dist^2)
        //              == (radv / dist) * (Gm / dist^2)
        //              == radv * (Gm / dist^3)
        p.acc += radv * (G * other.mass / r3);
    }

    p.vel += world.dt * p.acc;
    p.pos += world.dt * p.vel;

    new.arr[i] = p;
}
