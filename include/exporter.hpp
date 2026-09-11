#pragma once

#include "run_options.hpp"
#include "gl_framebuffer.hpp"
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class RenderPass;
class VideoWriter;

// 落盘的东西都归这儿：PNG 截图、buffer dump、WAV、视频。渲染侧只管把画面准备到
// 输出目标里，什么时候存、存成什么格式都是这里的事
class Exporter {
public:
    explicit Exporter(const RunOptions& options);
    ~Exporter();

    // 输出目标换了（重建或窗口缩放）得说一声：截图和视频都从它这儿读像素
    void setOutputTarget(GLFramebuffer* target, int width, int height);

    // 视频。打开失败返回 false，调用方直接放弃这次导出
    bool beginVideo(bool hasAudio);
    void writeVideoFrame();
    void endVideo();

    void captureFrame(int frameIndex);                     // 输出目标 -> PNG
    void captureBuffer(RenderPass& pass, int frameIndex);  // buffer 的浮点 -> PNG

    // 这一帧的音频，交错立体声。播放不归这里管，音轨和 WAV 各判各的去向
    void appendAudio(const std::vector<int16_t>& samples);
    void writeAudioDump();

    // 导出收尾：写音频、报耗时。耗时由程序自己报——外部的 time 在 Windows 上
    // 量不到原生进程，给的数不能用
    void finish(const std::string& label, int renderedFrames,
                std::chrono::steady_clock::time_point start);

private:
    void readOutputPixels(std::vector<uint8_t>& pixels);  // 输出目标，bottom-up RGBA8

    const RunOptions& m_options;
    GLFramebuffer* m_outputTarget = nullptr;
    int m_width = 0;
    int m_height = 0;

    std::unique_ptr<VideoWriter> m_videoWriter;
    std::vector<int16_t> m_audioDumpSamples;
    std::vector<uint8_t> m_pixelBuffer;  // 读像素的中转缓冲，复用免得每帧重新分配
};
