// 左半屏显示 BufferA，右半屏显示 BufferB
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    float a = texelFetch(iChannel0, ivec2(fragCoord), 0).r * 255.0;
    float b = texelFetch(iChannel1, ivec2(fragCoord), 0).r * 255.0;
    fragColor = vec4(((uv.x < 0.5) ? a : b) / 255.0, 0.0, 0.0, 1.0);
}
