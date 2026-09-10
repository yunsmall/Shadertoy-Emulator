// 读 BufferA 并放大 10 倍：用来分辨读到的是本帧还是上一帧的 A
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float a = texelFetch(iChannel0, ivec2(fragCoord), 0).r * 255.0;
    fragColor = vec4(clamp(a * 10.0, 0.0, 255.0) / 255.0, 0.0, 0.0, 1.0);
}
