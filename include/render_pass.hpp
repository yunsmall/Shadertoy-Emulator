#pragma once

#include "gl_framebuffer.hpp"
#include "pass_config.hpp"
#include <SFML/Graphics.hpp>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

// 一个 pass 的运行时状态：shader、渲染目标、输入通道，以及编译后预查的 uniform location。
// Shadertoy 把 pass 分三类，靠 isImage / isSound 区分：
//   buffer —— 渲染进自己的 FBO，能被别的 pass 采样，内容是浮点
//   image  —— 直接画到输出目标，没有自己的 FBO
//   sound  —— 输出 vec2 音频，不产生画面
struct RenderPass {
    std::string name;
    sf::Shader shader;
    std::unique_ptr<GLFramebuffer> framebuffer;      // 主 FBO（浮点纹理）
    std::unique_ptr<GLFramebuffer> framebufferAlt;   // 双缓冲备用
    std::array<std::optional<ChannelInput>, 4> channels;
    int width = 0;
    int height = 0;
    bool useWindowResolution = false;  // 是否使用窗口分辨率
    bool useDoubleBuffer = false;
    int currentBuffer = 0;
    bool isImage = false;
    bool isSound = false;

    // ANGLE 翻译时会给标识符加前缀（iResolution 变成 _uiResolution），这里是原名
    // 到驱动里实际名字的对照表。设 uniform 和查 location 都要过它，每帧都查，
    // 所以用 unordered_map
    std::unordered_map<std::string, std::string> nameMap;

    // 编译后预查的 uniform location。-1 表示 shader 里没有这个 uniform
    // （GLSL 编译器会把没用到的优化掉），这时不能调 setUniform，否则 SFML 每帧刷一行警告
    GLint locIResolution = -1, locITime = -1, locITimeDelta = -1, locIFrame = -1;
    GLint locIFrameRate = -1, locIMouse = -1, locIDate = -1;
    GLint locChannelResolution = -1, locChannelTime = -1;
    GLint locISampleRate = -1, locISampleOffset = -1;
    std::array<GLint, 4> locChannels = {-1, -1, -1, -1};

    // 双缓冲：往一个写、从另一个读。自引用的 pass（采样自己）这样才能读到上一帧的
    // 内容，而不是本帧刚写了一半的
    GLFramebuffer* getWriteTarget() {
        return useDoubleBuffer ? (currentBuffer == 0 ? framebuffer.get() : framebufferAlt.get())
                               : framebuffer.get();
    }
    GLFramebuffer* getReadTarget() {
        return useDoubleBuffer ? (currentBuffer == 0 ? framebufferAlt.get() : framebuffer.get())
                               : framebuffer.get();
    }
    void swapBuffer() {
        if (useDoubleBuffer) currentBuffer = 1 - currentBuffer;
    }
};
