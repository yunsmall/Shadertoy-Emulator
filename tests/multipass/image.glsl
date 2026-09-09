// 左半屏显示 BufferA 记录的 iFrame，右半屏显示本 pass 自己的 iFrame。
// 两者必须完全相同——不同就说明 buffer 和 image 差了一帧。
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float bufferFrame = texelFetch(iChannel0, ivec2(0, 0), 0).r * 255.0;
    float imageFrame = float(iFrame);

    vec2 uv = fragCoord / iResolution.xy;
    float v = (uv.x < 0.5) ? bufferFrame : imageFrame;
    fragColor = vec4(v / 255.0, 0.0, 0.0, 1.0);
}
