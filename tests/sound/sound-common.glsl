// 用 common.glsl 里的 tone()，顺带验证宏 TAU 也能透过来。
// 左右频率不同，FFT 分得开
vec2 mainSound(int samp, float time) {
    return vec2(tone(440.0, time, 0.30), tone(700.0, time, 0.40));
}
