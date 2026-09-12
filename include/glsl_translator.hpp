#pragma once

#include <string>
#include <unordered_map>

struct TranslatedShader {
    bool ok = false;
    std::string code;  // 翻译后的桌面 GLSL
    // uniform 原名 -> 驱动里的名字。用 unordered_map 是因为它每帧设 uniform 时都要查
    std::unordered_map<std::string, std::string> uniforms;
    std::string log;  // 失败原因
};

// 把 ESSL 交给 ANGLE 翻译成桌面 GLSL，顺带补齐 WebGL 的语义。最要紧的是未初始化
// 变量补 0：GLSL 规范里那是 undefined，桌面驱动给的是寄存器残值（实测 float/vec3
// 拿到的都不是 0），而 Shadertoy 跑在 WebGL 上，不少 shader 踩着这条保证写。
//
// 翻译过程会给标识符加前缀防撞驱动内建名（iResolution 变成 _uiResolution），
// uniforms 是原名到实际名字的对照表，设 uniform 和查 location 都得过它
TranslatedShader translateShader(const std::string& esslSource);
