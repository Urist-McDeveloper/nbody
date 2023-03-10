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

layout (std140, binding = 0) readonly uniform WorldData {
    uint size;
    float dt;
} world;

layout (std140, binding = 1) buffer FrameData {
    Body bodies[];
} frame;

layout (local_size_x = 16, local_size_y = 1, local_size_z = 1) in;
