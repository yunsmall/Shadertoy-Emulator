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

    // Images 模式。可以给多个 --images（逗号分隔也行），每段是一个 Python 切片——
    // 只按固定间隔挑帧太局限，比如想取几个关键帧再补一段密集的。段之间允许重叠，
    // 重复的帧只处理一次
    std::vector<FrameRange> imageRanges;
    std::filesystem::path imageDir;
    // 导出图片默认就会做依赖分析：无状态的通道跳过，有状态的通道和它们读到的照常
    // 每帧渲染（见 RenderCore::computeMustRunPasses），结果和全渲染一致。
    // 下面这个开关反过来，一帧不落地全渲染——给不信赖分析结果的场合用
    bool renderAllFrames = false;
    // 不做依赖分析，中间帧一律不渲染。有帧间状态的 shader 会算错，快但危险
    bool forceSkipIntermediate = false;

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
