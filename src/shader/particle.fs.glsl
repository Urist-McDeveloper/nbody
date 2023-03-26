#version 450

layout (location = 0) in GeomData {
    vec2 in_coord;
    vec3 in_color;
};

layout (location = 0) out vec4 out_color;

const float F = 0.02;

void main() {
    float sq_mag = dot(in_coord, in_coord);
    out_color = vec4(in_color, (1.0 - sq_mag) / F);
}
