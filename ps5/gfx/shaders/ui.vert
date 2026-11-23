#version 450
// Lantern Miner PS5: 2D primitives. Positions are render-target pixels; the fragment shader decides what a vertex draws.

layout(push_constant) uniform PC {
    vec2 target;
    float gamma;
    float pad;
} pc;

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;
layout(location = 3) in vec4 a_aux;    // half width, half height, corner radius, mode
layout(location = 4) in float a_extra; // shadow blur, ring thickness, glow mid alpha

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;
layout(location = 2) out vec4 v_aux;
layout(location = 3) out float v_extra;

void main()
{
    gl_Position = vec4(a_pos / pc.target * 2.0 - 1.0, 0.0, 1.0);
    v_uv = a_uv;
    v_color = a_color;
    v_aux = a_aux;
    v_extra = a_extra;
}
