// 上半屏：BufferA 里看到的 iResolution；下半屏：Image 里看到的 iChannelResolution[0]
// 前者验证 buffer 用自己的分辨率，后者验证通道分辨率正确报出被采样纹理的尺寸
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    if (uv.y > 0.5) {
        fragColor = vec4(texture(iChannel0, uv).rg, 0.0, 1.0);
    } else {
        fragColor = vec4(iChannelResolution[0].xy / 128.0, 0.0, 1.0);
    }
}
