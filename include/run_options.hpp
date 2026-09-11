#pragma once

#include "frame_range.hpp"
#include <filesystem>
#include <string>
#include <vector>

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
    // 只渲染被选中的帧，跳过中间的。前提是 shader 没有帧间状态（buffer 反馈），
    // 有的话中间帧就是它状态的来源，跳掉结果不对——这一点由使用者自己判断
    bool skipIntermediate = false;

    // Images 模式下额外把这几个 buffer 也导成 PNG，写进 <imageDir>/buffers/<名字>/。
    // 名字是 BufferA 这种 pass 名；"all" 表示所有 buffer pass
    std::vector<std::string> dumpBuffers;
    // 导出 buffer 前先乘这个数再 clamp 到 0..1。buffer 是浮点的，暗部的中间值
    // 直接看就是一片黑，给个增益才看得出来
    float dumpBufferGain = 1.0f;

    // Video 模式
    std::filesystem::path videoPath;
    int videoSeconds = 0;

    // 与模式正交：额外把 Sound pass 的输出写成 WAV
    std::filesystem::path audioDumpPath;

    // 调试：把指定 pass 的 buffer 内容当画面显示/导出，而不是 Image pass。
    // 窗口模式下 ImGui 里也能随时改，这里给的是初始值，顺便让离屏模式也能用上
    std::string debugViewPass;

    bool isOffscreen() const { return mode != RunMode::Window; }
};
