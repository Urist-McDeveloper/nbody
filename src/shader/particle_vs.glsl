#version 450

struct Particle {
    vec2 pos, vel, acc;
    float mass, radius;
};

layout (std140, binding = 0) readonly buffer Data {
    Particle particles[];
};

layout (location = 0) out vec3 color;

void main() {
    Particle particle = particles[gl_VertexIndex];
    gl_Position = vec4(
        particle.pos / 40000.f,
        0.f, 1.f
    );
    gl_PointSize = 1.f;
    color = vec3(1.f, 1.f, 1.f);
}
