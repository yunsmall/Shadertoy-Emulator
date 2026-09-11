// 把 B 的累加和缩回 0..255 显示
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float sum = texelFetch(iChannel0, ivec2(fragCoord), 0).r;
    fragColor = vec4(vec3(sum / 255.0), 1.0);
}
