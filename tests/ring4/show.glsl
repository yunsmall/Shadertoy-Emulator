// 四分屏：A/B/C/D 依次从左到右
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    float a = texelFetch(iChannel0, ivec2(fragCoord), 0).r * 255.0;
    float b = texelFetch(iChannel1, ivec2(fragCoord), 0).r * 255.0;
    float c = texelFetch(iChannel2, ivec2(fragCoord), 0).r * 255.0;
    float d = texelFetch(iChannel3, ivec2(fragCoord), 0).r * 255.0;
    float v = (uv.x < 0.25) ? a : ((uv.x < 0.5) ? b : ((uv.x < 0.75) ? c : d));
    fragColor = vec4(v / 255.0, 0.0, 0.0, 1.0);
}
