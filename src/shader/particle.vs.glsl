#version 450

struct Particle {
    vec2 pos, vel, acc;
    float mass, radius;
};

layout (std140, binding = 0) readonly buffer Data {
    Particle particles[];
} data;

layout (push_constant) uniform PushConstants {
    mat3x2 camera_proj;
    vec2 dims;
    float dt;
} push;

layout (location = 0) out GeomData {
    vec2 out_size;
    vec3 out_color;
};

const vec3 EP_COLOR = vec3(145, 145, 233) / 255.f;
const vec3 NP_COLOR = vec3(175, 195, 175) / 255.f;
const vec3 GC_COLOR = vec3(222, 222, 222) / 255.f;

layout (constant_id = 1) const float MIN_GC_MASS = 0;

void main() {
    Particle p = data.particles[gl_VertexIndex];

    vec2 vel = p.vel + push.dt * p.acc;
    vec2 pos = p.pos + push.dt * vel;
    vec2 proj_pos = push.camera_proj * vec3(pos, 1);
    gl_Position = vec4(proj_pos, 0, 1);

    vec2 proj_size = push.camera_proj * vec3(p.radius, p.radius, 0);
    out_size = max(proj_size, 1.0 / push.dims);

    if (p.mass <= 0) {
        out_color = EP_COLOR;
    } else if (p.mass < MIN_GC_MASS) {
        out_color = NP_COLOR;
    } else {
        out_color = GC_COLOR;
    }
}
