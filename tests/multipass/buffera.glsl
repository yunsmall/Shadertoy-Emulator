// 把本 pass 看到的 iFrame 编码成颜色，供 Image pass 核对
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    fragColor = vec4(float(iFrame) / 255.0, 0.0, 0.0, 1.0);
}
