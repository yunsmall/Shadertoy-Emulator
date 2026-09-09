// 读 BufferA 本帧的值 +1（BufferA 先渲染）
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float cur = texelFetch(iChannel0, ivec2(fragCoord), 0).r * 255.0;
    fragColor = vec4((cur + 1.0) / 255.0, 0.0, 0.0, 1.0);
}
