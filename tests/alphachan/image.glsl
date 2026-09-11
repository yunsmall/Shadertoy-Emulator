// 灰度显示 BufferA 的 alpha（除以 16 是为了落进 [0,1]，5 帧最多到 5）。
// 自己的 alpha 故意写 0：Image 通道的包装必须把它盖回 1，否则窗口和导出的 PNG
// 会带上透明的 alpha，看着就是全透明
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float a = texelFetch(iChannel0, ivec2(fragCoord), 0).a;
    fragColor = vec4(vec3(a / 16.0), 0.0);
}
