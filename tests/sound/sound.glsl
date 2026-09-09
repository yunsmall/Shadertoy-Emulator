// 输出 440Hz 正弦波（幅度 0.5），方便脚本验证频率和幅度
vec2 mainSound(int samp, float time) {
    float v = sin(6.2831853 * 440.0 * time) * 0.5;
    return vec2(v, v);
}
