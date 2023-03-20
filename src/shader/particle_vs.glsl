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
} push;

layout (location = 0) out vec3 color;

void main() {
    Particle p = data.particles[gl_VertexIndex];

    mat3x2 matrix = push.camera_proj;
    vec2 position = matrix * vec3(p.pos, 1);

    gl_Position = vec4(position, 0, 1);
    gl_PointSize = 1.f;
    color = vec3(1.f, 1.f, 1.f);
}
