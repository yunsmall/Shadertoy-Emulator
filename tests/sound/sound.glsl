// 左右声道各给几个互不重叠的正弦。频率全取整数：1 秒的窗口正好装下整数个周期，
// 能量不会漏到邻 bin，峰值的位置和高度都能直接断言。
// 两边刻意不共用频率，顺带能验证立体声没串道
vec2 mainSound(int samp, float time) {
    float left = sin(6.2831853 * 440.0 * time) * 0.30
               + sin(6.2831853 * 1000.0 * time) * 0.20
               + sin(6.2831853 * 3000.0 * time) * 0.10;
    float right = sin(6.2831853 * 700.0 * time) * 0.40
                + sin(6.2831853 * 2000.0 * time) * 0.25;
    return vec2(left, right);
}
