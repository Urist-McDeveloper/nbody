#version 450

layout (points) in;

layout (triangle_strip, max_vertices = 4) out;

layout (location = 0) in VertData {
    vec2 size;
    vec3 color;
} vs_in[];

layout (location = 0) out FragData {
    vec2 out_coord;
    vec3 out_color;
};

void main() {
    vec4 pos = gl_in[0].gl_Position;
    vec3 color = vs_in[0].color;
    vec2 size = vs_in[0].size;

    // bottom-left
    gl_Position = vec4(pos.x - size.x, pos.y + size.y, 0, 1);
    out_coord = vec2(-1.0, 1.0);
    out_color = color;
    EmitVertex();

    // bottom-right
    gl_Position = vec4(pos.x + size.x, pos.y + size.y, 0, 1);
    out_coord = vec2(1.0, 1.0);
    out_color = color;
    EmitVertex();

    // top-left
    gl_Position = vec4(pos.x - size.x, pos.y - size.y, 0, 1);
    out_coord = vec2(-1.0, -1.0);
    out_color = color;
    EmitVertex();

    // top-right
    gl_Position = vec4(pos.x + size.x, pos.y - size.y, 0, 1);
    out_coord = vec2(1.0, -1.0);
    out_color = color;
    EmitVertex();

    EndPrimitive();
}
