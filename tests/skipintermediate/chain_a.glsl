// 无状态：只把帧号写进去，自己不需要任何历史。但它被 BufferB 逐帧累加，所以依赖
// 分析必须把它也算进"每帧都要渲染"——漏了它，B 在中间帧就会累加到上一次留下的旧值
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    fragColor = vec4(float(iFrame), 0.0, 0.0, 1.0);
}
