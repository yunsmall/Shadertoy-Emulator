// 读 iChannel0 的值 +1 写回
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float prev = texelFetch(iChannel0, ivec2(fragCoord), 0).r * 255.0;
    fragColor = vec4((prev + 1.0) / 255.0, 0.0, 0.0, 1.0);
}
