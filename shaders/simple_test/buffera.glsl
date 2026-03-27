// BufferA: 输出红色渐变
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    // 红色从左到右渐变
    fragColor = vec4(uv.x, 0.0, 0.0, 1.0);
}
