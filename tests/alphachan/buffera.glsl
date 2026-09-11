// 拿 alpha 当数据用：每帧在上一帧的 alpha 上加 1，帧 N 应该是 N+1。
// Buffer 的包装若把 alpha 抹成 1（照搬 Image 那套），这里每帧读回的都是 1，累加不起来。
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float prev = texelFetch(iChannel0, ivec2(fragCoord), 0).a;
    fragColor = vec4(0.0, 0.0, 0.0, prev + 1.0);
}
