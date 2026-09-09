// 用各种预处理指令算出最终颜色，验证预处理器（内置 / 外部都跑一遍对照）
// 期望颜色：R = 0.25, G = 0.5, B = 0.625
#define BASE 0.25
#define SCALE(x) ((x) * 2.0)

#ifndef MISSING_MACRO
    #define ADDED 0.5
#endif

#ifdef MISSING_MACRO
    #define WRONG 1.0
#else
    #define WRONG 0.0
#endif

#define TEMP 0.75
#undef TEMP

#ifdef TEMP
    #define UNDEF_WORKED 0.0
#else
    #define UNDEF_WORKED 1.0
#endif

#include "helper.glsl"

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // R: 简单宏
    // G: 带参数的宏
    // B: #include 引入的宏 + #undef 是否真的生效（生效时 UNDEF_WORKED = 1）
    fragColor = vec4(BASE, SCALE(BASE), HELPER_VALUE + UNDEF_WORKED * 0.5 + WRONG * 0.1, 1.0);
}
