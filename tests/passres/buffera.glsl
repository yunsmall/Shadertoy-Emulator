// 这个 pass 的渲染目标被 config 指定为 32x32，而窗口是 64x64。
// 把 iResolution 编码进像素，用来验证 buffer 内看到的是自己的分辨率而不是窗口的：
//   红 = iResolution.x / 128，绿 = iResolution.y / 128
// 32 应该给出 0.25（读回 64），若错误地拿到窗口尺寸则是 0.5（读回 128）
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    fragColor = vec4(iResolution.x / 128.0, iResolution.y / 128.0, 0.0, 1.0);
}
