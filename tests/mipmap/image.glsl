// 采样第 5 级 mipmap。如果 Buffer 的 mipmap 没生成，纹理不完整，
// 按 GL 规范采样会返回黑色 —— 于是这里就能区分出来。
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    fragColor = vec4(textureLod(iChannel0, uv, 5.0).rgb, 1.0);
}
