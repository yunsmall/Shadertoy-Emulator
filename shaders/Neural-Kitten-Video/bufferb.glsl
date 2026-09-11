/*

Created by Tonny Espeset, 2026
https://www.shadertoy.com/view/NcKXW3

Neural Kitten Video v4

Feel free to use this code, but please keep this credit and link.

Check my other shaders at

https://www.shadertoy.com/user/Espeset

*/
const float VID_LEN = 6.072733;
const float PLAY_RATE = 1.60160; // clip fps over the 29.97 fps training clock, played at 80% speed
const float EASE = 1.00; // seconds of slow-down into each turn, and back up to speed
const int VW = 480, VH = 270; // the video is decoded once per frame at this size, bottom-left

const float DQ[7] = float[7](0.0061691622, 0.022814814, 0.023601754, 0.034331501, 0.0016137771, 0.00032427808, 0.0026378452);
const float B0[32] = float[32](-0.0017825754, -0.013242581, -0.017534209, 0.0044938014, -0.024837228, 0.0013983703, 0.010343289, -0.054948628, 0.0011742937, -0.0041055679, 0.0013779331, 0.040018093, 0.0043162974, 0.011465782, 0.028039228, -0.0016417588, -0.024485487, -0.012858225, -0.0002860789, -0.0030848586, 0.0019480463, 0.016453128, -0.037122652, -0.014290443, -0.0011321058, -0.02395064, 0.026967829, -0.015596452, 0.019341456, -0.011004416, 0.026417555, -0.0079044988);
const float B1[32] = float[32](-0.051286161, -0.0089407936, -0.02180597, 0.041777533, 0.007900768, -0.011047595, 0.0027147019, 0.0077635101, -0.028989749, 0.021950837, -0.030517295, 0.012387306, -0.030164968, -0.021625649, 0.037806638, 0.012514141, 0.025457386, 0.013043151, -0.0094222771, 0.004348564, 0.027597582, 4.343417e-06, 0.0066645751, 0.027632721, 0.023591539, -0.022455808, -0.010238332, 0.025288967, 0.042225864, -0.014122969, -0.0040745055, -0.029470662);
const float B2[3] = float[3](0.22200443, -0.080410451, -0.14089912);

// texel k of Buffer A: four int8 of the weight stream, scaled by the tensor's DQ at the call site
vec4 nnTex(int k)
{
    int W = int(iResolution.x);
    return texelFetch(iChannel0, ivec2(k - (k / W) * W, k / W), 0);
}

float nnNear(float d) { return 1.25 * d * d * d - 2.25 * d * d + 1.0; }
float nnFar(float d) { return -0.75 * d * d * d + 3.75 * d * d - 6.0 * d + 3.0; }

// Keys cubic (a = -0.75, same as PyTorch bicubic) weights for the 4 taps at -1, 0, 1, 2
vec4 nnKernel(float x)
{
    float x2 = x * x, x3 = x2 * x;
    return vec4(nnFar(x + 1.0), nnNear(x), nnNear(1.0 - x), nnFar(2.0 - x));
}

// 4x4 cubic tap of a plane, T texels per cell, align_corners, edge clamp
void planeSample(int base, int W, int H, int T, float dq, float u, float v, inout vec4 acc[4])
{
    for (int k = 0; k < 4 + min(iFrame, 0); k++) acc[k] = vec4(0.0);
    float fu = clamp(u, 0.0, 1.0) * float(W - 1);
    float fv = clamp(v, 0.0, 1.0) * float(H - 1);
    int c0 = clamp(int(floor(fu)), 0, W - 1);
    int r0 = clamp(int(floor(fv)), 0, H - 1);
    vec4 wu = nnKernel(fu - float(c0));
    vec4 wv = nnKernel(fv - float(r0));
    for (int q = 0; q < 16 + min(iFrame, 0); q++)
    {
        int col = clamp(c0 - 1 + (q & 3), 0, W - 1);
        int row = clamp(r0 - 1 + (q >> 2), 0, H - 1);
        float wgt = wu[q & 3] * wv[q >> 2];
        int o = base + (row * W + col) * T;
        for (int k = 0; k < T + min(iFrame, 0); k++) acc[k] += wgt * (nnTex(o + k) * dq);
    }
}

vec3 nnVideo(vec2 uv, float tSec)
{
    float t = clamp(tSec / VID_LEN, 0.0, 1.0);
    float x = clamp(uv.x, 0.0, 1.0);
    float y = clamp(uv.y, 0.0, 1.0);
    vec4 f[5];
    vec4 a[4], b[4], c[4];
    planeSample(0, 52, 29, 2, DQ[0], x, y, a);
    for (int k = 0; k < 2 + min(iFrame, 0); k++) f[k] = a[k];
    planeSample(3016, 26, 15, 3, DQ[1], x, y, a);
    planeSample(4186, 26, 30, 3, DQ[2], x, t, b);
    planeSample(6526, 15, 30, 3, DQ[3], y, t, c);
    for (int k = 0; k < 3 + min(iFrame, 0); k++) f[2 + k] = a[k] * b[k] * c[k];
    vec4 h0[8];
    for (int o = 0; o < 32 + min(iFrame, 0); o++)
    {
        float s = B0[o];
        for (int j = 0; j < 5 + min(iFrame, 0); j++) s += dot(nnTex(7876 + o * 5 + j) * DQ[4], f[j]);
        h0[o >> 2][o & 3] = sin(30.0 * s);
    }
    vec4 h1[8];
    for (int o = 0; o < 32 + min(iFrame, 0); o++)
    {
        float s = B1[o];
        for (int j = 0; j < 8 + min(iFrame, 0); j++) s += dot(nnTex(8036 + o * 8 + j) * DQ[5], h0[j]);
        h1[o >> 2][o & 3] = sin(30.0 * s);
    }
    vec3 rgb;
    for (int o = 0; o < 3 + min(iFrame, 0); o++)
    {
        float s = B2[o];
        for (int j = 0; j < 8 + min(iFrame, 0); j++) s += dot(nnTex(8292 + o * 8 + j) * DQ[6], h1[j]);
        rgb[o] = 1.0 / (1.0 + exp(-s));
    }
    return rgb;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    ivec2 p = ivec2(fragCoord);
    int W = int(iResolution.x), H = int(iResolution.y);
    if (p.x == 0 && p.y == H - 1)
    {
        // px(0, H-1) = playhead (tSec, dir, firstRun), read back from this buffer's previous frame
        vec4 s = texelFetch(iChannel1, ivec2(0, H - 1), 0);
        float tSec = s.x;
        float dir = s.y;
        float first = s.z;
        if (iFrame == 0 || dir == 0.0) { tSec = 0.0; dir = 1.0; first = 1.0; }
        float dt = clamp(iTimeDelta, 1.0/240.0, 1.0/20.0);
        // constant deceleration: speed goes with the square root of the distance to the turn
        float D = max(0.5*PLAY_RATE*EASE, 1e-6);
        float ahead = dir > 0.0 ? VID_LEN - tSec : tSec;
        float behind = VID_LEN - ahead;
        float sp = min(sqrt(ahead/D), first > 0.0 ? 1.0 : sqrt(behind/D));
        tSec += dir*dt*PLAY_RATE*clamp(sp, 0.05, 1.0);
        if (tSec > VID_LEN) { tSec = 2.0*VID_LEN - tSec; dir = -1.0; first = 0.0; }
        if (tSec < 0.0) { tSec = -tSec; dir = 1.0; first = 0.0; }
        fragColor = vec4(tSec, dir, first, 0.0);
        return;
    }
    ivec2 vid = ivec2(min(VW, W), min(VH, H - 1));
    if (p.x >= vid.x || p.y >= vid.y) { fragColor = vec4(0.0); return; }
    float tSec = texelFetch(iChannel1, ivec2(0, H - 1), 0).x;
    fragColor = vec4(nnVideo((vec2(p) + 0.5) / vec2(vid), tSec), 1.0);
}
