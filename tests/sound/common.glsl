// 这里放的是函数而不只是宏：纯宏展开后是空串，"common 被展开了两遍"这种错
// 靠宏是验不出来的，得有个实体定义才会撞成重复定义
#define TAU 6.2831853

float tone(float freq, float t, float amp) {
    return sin(TAU * freq * t) * amp;
}
