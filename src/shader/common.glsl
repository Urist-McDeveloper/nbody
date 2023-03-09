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

layout (std140, binding = 0) uniform UBO {
    uint size;
    float dt;
    vec2 min;
    vec2 max;
} ubo;

layout (std140, binding = 1) buffer SBO {
    Body bodies[];
} sbo;

layout (local_size_x = 16) in;
