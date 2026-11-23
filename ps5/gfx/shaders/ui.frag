#version 450
// Modes (v_aux.w):  0 textured quad   1 text (R8 atlas)   2 rounded rectangle   3 soft shadow   4 outline ring
//                   5 line segment with round caps (aux.xy = half vector, aux.z = thickness; uv = position from the centre)
//                   6 textured quad with rounded corners
//                   7 flat colour (rectangles, triangles; vertex colours interpolate = gradients)
//                   8 radial glow (uv = pixels from the centre, aux.x = radius, extra = alpha at the half radius)

layout(set = 0, binding = 0) uniform sampler2D tex;

layout(push_constant) uniform PC {
    vec2 target;
    float gamma;
    float pad;
} pc;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 2) in vec4 v_aux;
layout(location = 3) in float v_extra;
layout(location = 0) out vec4 o;

float sd_round_box(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

void main()
{
    int mode = int(v_aux.w + 0.5);
    vec4 c = v_color;
    if (mode == 0) {
        c *= texture(tex, v_uv);
    } else if (mode == 1) {
        c.a *= texture(tex, v_uv).r;
    } else if (mode == 7) {
        // flat
    } else if (mode == 10) {
        // vignette: aux.x/aux.y = inner/outer radius in target pixels, extra = alpha at the outer radius
        float t = clamp((length(v_uv) - v_aux.x) / max(v_aux.y - v_aux.x, 1.0), 0.0, 1.0);
        c.a *= t * v_extra;
    } else if (mode == 9) {
        // scanlines: aux.x = period in target pixels, extra = strength
        float s = 0.5 + 0.5 * cos(floor(gl_FragCoord.y) * 6.2831853 / max(v_aux.x, 1.0)); // whole pixel rows: 1,0 at a 2px period
        c.a *= v_extra * s;
    } else if (mode == 8) {
        float t = clamp(length(v_uv) / max(v_aux.x, 1.0), 0.0, 1.0);
        float a = t < 0.5 ? mix(1.0, v_extra, t * 2.0) : mix(v_extra, 0.0, (t - 0.5) * 2.0);
        c.a *= a;
    } else if (mode == 6) {
        vec2 p = (v_uv - 0.5) * 2.0 * v_aux.xy;
        float d = sd_round_box(p, v_aux.xy, v_aux.z);
        c *= texture(tex, v_uv);
        c.a *= clamp(0.5 - d, 0.0, 1.0);
    } else if (mode == 5) {
        vec2 a = v_aux.xy;
        vec2 pa = v_uv + a;
        vec2 ba = 2.0 * a;
        float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
        float d = length(pa - ba * h) - 0.5 * v_aux.z;
        c.a *= clamp(0.5 - d, 0.0, 1.0);
    } else {
        float d = sd_round_box(v_uv, v_aux.xy, v_aux.z);
        if (mode == 2) {
            c.a *= clamp(0.5 - d, 0.0, 1.0);
        } else if (mode == 3) {
            float b = max(v_extra, 1.0);
            float s = 1.0 - smoothstep(-0.35 * b, b, d);
            c.a *= s * s;
        } else {
            float t = v_extra;
            c.a *= clamp(0.5 - (abs(d + 0.5 * t) - 0.5 * t), 0.0, 1.0);
        }
    }
    o = vec4(pow(clamp(c.rgb, 0.0, 1.0), vec3(pc.gamma)), c.a);
}
