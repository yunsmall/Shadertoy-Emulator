// 自引用累加：把 BufferA 里的帧号一帧一帧加起来，帧 N 的值是 0+1+…+N
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float prev = texelFetch(iChannel0, ivec2(fragCoord), 0).r;
    float add = texelFetch(iChannel1, ivec2(fragCoord), 0).r;
    fragColor = vec4(prev + add, 0.0, 0.0, 1.0);
}
