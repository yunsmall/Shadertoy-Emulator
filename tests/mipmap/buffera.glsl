// 自引用 + 固定值，供 Image pass 用 mipmap 采样
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    fragColor = vec4(0.5, 0.25, 0.125, 1.0);
}
