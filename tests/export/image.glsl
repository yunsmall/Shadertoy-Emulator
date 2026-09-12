#include "shared.glsl"
#include "helper.glsl"

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;

    // 三个通道各验一路：R 走 common 的宏，G 走 #include 进来的宏，
    // B 走纹理；再掺一点 BufferA 的内容，导出前后的比对就连 buffer 一起覆盖了
    vec3 tex = texture(iChannel0, vec2(0.5)).rgb;
    vec3 buf = texture(iChannel1, uv).rgb;

    fragColor = vec4(COMMON_TINT + buf.r * SHARED_GAIN, HELPER_BIAS, tex.b, 1.0);
}
