// 原样显示 BufferA 的累加结果
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    fragColor = vec4(texelFetch(iChannel0, ivec2(fragCoord), 0).r, 0.0, 0.0, 1.0);
}
