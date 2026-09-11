// 自引用（双缓冲）：读自己上一帧的输出并 +1。这里不是要验双缓冲，是拿它当
// "渲染了几帧"的计数器——跳过一个中间帧，这个值就少 1
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float prev = texelFetch(iChannel0, ivec2(fragCoord), 0).r * 255.0;
    fragColor = vec4((prev + 1.0) / 255.0, 0.0, 0.0, 1.0);
}
