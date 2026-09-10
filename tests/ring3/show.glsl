// 三分屏：左 BufferA，中 BufferB，右 BufferC
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    float a = texelFetch(iChannel0, ivec2(fragCoord), 0).r * 255.0;
    float b = texelFetch(iChannel1, ivec2(fragCoord), 0).r * 255.0;
    float c = texelFetch(iChannel2, ivec2(fragCoord), 0).r * 255.0;
    float v = (uv.x < 0.333) ? a : ((uv.x < 0.667) ? b : c);
    fragColor = vec4(v / 255.0, 0.0, 0.0, 1.0);
}
