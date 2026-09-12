// 刁钻声明的集合。按 PNG 的行号对号入座，每行一个检查，行号对应 check.py 里的
// CHECKS 下标。这里既测"该补的补了"，也测"不该动的没动"——后者一旦被改就会
// 编译失败，所以能跑起来本身就是检查

// 注释里的假声明不能被当成真的：float ghost;
/* 块注释里也有：vec3 phantom; */
#define FAKE_DECL float fake

struct Pair {          // 成员不能带初值
    float a;
    vec3 b;
};

uniform float uBias;   // uniform 禁止初始化
const float PI = 3.14159;

float helper(float x);          // 函数原型也不能被当成变量声明

float helper(float x) {
    float k;                    // 函数里的局部未初始化变量
    k = 2.0;
    return x * k;
}

float gFloat;                   // 全局未初始化
vec3 gVec;
ivec2 gIVec;

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    int idx = int(fragCoord.x);   // 用 x 而不是 y：PNG 的上下是翻过的，水平方向没有

    float a;
    float b, c, d, e, f;
    float g,
          h,
          i2;
    float x1 = 1.5, x2;
    vec2 v2;
    vec3 v3;
    vec4 v4;
    ivec2 iv;
    uvec2 uv;
    bvec2 bv;
    mat2 m2;
    mat3x4 m34;
    highp float hp;
    bool flag;
    int n;
    uint u;
    float withCall = max(1.0, 2.0), afterCall;
    float withParen = (1.0 + 2.0) * 3.0, afterParen;
    float withArray[2], afterArray;
    float zz[2] = float[2](1.0, 2.0), afterInitArray;
    float tern = true ? 1.0 : 2.0, afterTern;
    float sci = 1e-3, afterSci;
    float v1, v2_;
    float q1; float q2;
    float cm; // 紧跟注释，分号也在注释外面
    /* 前面是块注释 */ float bm;
    const int CN = 3;

    float inIf = 0.0;
    if (idx > -1) {
        float blockLocal;
        inIf = blockLocal;
    }
    float inFor = 0.0;
    for (int fi = 0; fi < 1; ++fi) {
        float loopLocal;
        inFor += loopLocal;
    }

    Pair p;
    p.a = 0.5;
    p.b = vec3(0.25);

    float ok = 0.0;
    if (idx == 0) ok = (a == 0.0) ? 1.0 : 0.0;
    else if (idx == 1) ok = (b == 0.0 && c == 0.0 && d == 0.0 && e == 0.0 && f == 0.0) ? 1.0 : 0.0;
    else if (idx == 2) ok = (g == 0.0 && h == 0.0 && i2 == 0.0) ? 1.0 : 0.0;
    else if (idx == 3) ok = (x1 == 1.5 && x2 == 0.0) ? 1.0 : 0.0;
    else if (idx == 4) ok = (v2 == vec2(0.0) && v3 == vec3(0.0) && v4 == vec4(0.0)) ? 1.0 : 0.0;
    else if (idx == 5) ok = (iv == ivec2(0) && uv == uvec2(0u) && bv == bvec2(false)) ? 1.0 : 0.0;
    else if (idx == 6) ok = (m2 == mat2(0.0) && m34 == mat3x4(0.0)) ? 1.0 : 0.0;
    else if (idx == 7) ok = (hp == 0.0) ? 1.0 : 0.0;
    else if (idx == 8) ok = (gFloat == 0.0 && gVec == vec3(0.0) && gIVec == ivec2(0)) ? 1.0 : 0.0;
    else if (idx == 9) ok = (flag == false && n == 0 && u == 0u) ? 1.0 : 0.0;
    else if (idx == 10) ok = (withCall == 2.0 && afterCall == 0.0) ? 1.0 : 0.0;
    else if (idx == 11) ok = (withParen == 9.0 && afterParen == 0.0) ? 1.0 : 0.0;
    else if (idx == 12) ok = (afterArray == 0.0) ? 1.0 : 0.0;   // 数组本身跳过，它后面的变量照补
    else if (idx == 13) ok = (zz[1] == 2.0 && afterInitArray == 0.0) ? 1.0 : 0.0;
    else if (idx == 14) ok = (tern == 1.0 && afterTern == 0.0) ? 1.0 : 0.0;
    else if (idx == 15) ok = (afterSci == 0.0 && sci > 0.0009 && sci < 0.0011) ? 1.0 : 0.0;
    else if (idx == 16) ok = (v1 == 0.0 && v2_ == 0.0) ? 1.0 : 0.0;
    else if (idx == 17) ok = (inIf == 0.0) ? 1.0 : 0.0;
    else if (idx == 18) ok = (inFor == 0.0) ? 1.0 : 0.0;
    else if (idx == 19) ok = (helper(1.0) == 2.0) ? 1.0 : 0.0;
    else if (idx == 20) ok = (CN == 3 && PI == 3.14159 && uBias == 0.0) ? 1.0 : 0.0;
    else if (idx == 21) ok = (q1 == 0.0 && q2 == 0.0) ? 1.0 : 0.0;
    else if (idx == 22) ok = (cm == 0.0 && bm == 0.0) ? 1.0 : 0.0;
    else if (idx == 23) ok = (p.a == 0.5 && p.b == vec3(0.25)) ? 1.0 : 0.0;
    else ok = 1.0;

    fragColor = vec4(ok, 0.0, 0.0, 1.0);
}
