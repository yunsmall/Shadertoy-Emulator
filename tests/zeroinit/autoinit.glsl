// 未初始化变量补零的测试。下面每个变量都故意不写初值：本地跑的不是 WebGL，
// 桌面驱动给的是寄存器残值（实测 float/vec3 都不是 0），补零之后才应当是 0。
// 12 项检查汇总进红色通道，全对是 1.0

// 注释里写的 "float ghost;" 不能被当成真声明： /* 也不能被当成块注释外的代码 */

struct Pair {         // 成员声明不能带初值，改写要整块放过
    float a;
    vec3 b;
};

uniform float uBias;  // uniform 禁止初始化
const float PI = 3.14159;  // 有初值，不该被动

float scale2(float x) {
    float k;          // 函数里的局部变量
    k = 2.0;
    return x * k;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float d;
    vec3 v;
    int n;
    bool flag;
    float a, b, c5, d5, e5;    // 五项挤在一行，全都没初值
    float m1,
          m2,
          m3;                  // 跨行的多声明
    float kept = 0.25;    // 有初值
    float keptA = 1.0, keptB;   // 前一个有初值，后一个没有
    Pair p;               // 自定义 struct 类型，没有通用零值构造

    p.a = 0.5;
    p.b = vec3(0.5);

    float acc = 0.0;
    for (int i = 0; i < 3; ++i) {
        acc += 1.0;
    }

    float ok = 0.0;
    ok += (d == 0.0) ? 1.0 : 0.0;
    ok += (v.x == 0.0) ? 1.0 : 0.0;
    ok += (n == 0) ? 1.0 : 0.0;
    ok += (flag == false) ? 1.0 : 0.0;
    ok += (a == 0.0 && b == 0.0 && c5 == 0.0 && d5 == 0.0 && e5 == 0.0) ? 1.0 : 0.0;
    ok += (m1 == 0.0 && m2 == 0.0 && m3 == 0.0) ? 1.0 : 0.0;
    ok += (kept == 0.25) ? 1.0 : 0.0;
    ok += (keptA == 1.0 && keptB == 0.0) ? 1.0 : 0.0;
    ok += (uBias == 0.0) ? 1.0 : 0.0;
    ok += (scale2(1.0) == 2.0) ? 1.0 : 0.0;
    ok += (PI == 3.14159) ? 1.0 : 0.0;
    ok += (acc == 3.0) ? 1.0 : 0.0;

    fragColor = vec4(ok / 12.0, 0.0, 0.0, 1.0);
}
