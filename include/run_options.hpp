#pragma once

#include "frame_range.hpp"
#include <filesystem>

// 三种模式互斥。以前是靠"某个参数在不在"来推断模式（captureRange.stop > 0 当
// 要不要导出的开关），参数一多就分不清意图了，所以改成显式枚举
enum class RunMode {
    Window,  // 实时渲染 + 交互窗口
    Images,  // 导出 PNG 序列
    Video,   // 导出视频（带音轨）
};

struct RunOptions {
    RunMode mode = RunMode::Window;
    bool showFps = false;
    bool enableGui = false;

    // 导出模式的帧率：图片模式决定 iTime 每帧走多少，视频模式还决定输出帧率
    float fps = 60.0f;

    // Images 模式
    FrameRange imageRange;
    std::filesystem::path imageDir;

    // Video 模式
    std::filesystem::path videoPath;
    int videoSeconds = 0;

    // 与模式正交：额外把 Sound pass 的输出写成 WAV
    std::filesystem::path audioDumpPath;

    bool isOffscreen() const { return mode != RunMode::Window; }
};
