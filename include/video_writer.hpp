#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

// 用 ffmpeg 把渲染结果编码成 mp4（H.264 视频 + AAC 音频）。
// ffmpeg 的头文件又大又是纯 C，所以具体实现藏在 Impl 里，对外只留这个轻量接口。
class VideoWriter {
public:
    VideoWriter();
    ~VideoWriter();
    VideoWriter(const VideoWriter&) = delete;
    VideoWriter& operator=(const VideoWriter&) = delete;

    // 输出尺寸会被对齐到偶数（H.264 用的 YUV420P 要求宽高都是偶数）。
    // withAudio 为 false 时只建视频流
    bool open(const std::filesystem::path& path, int width, int height, int fps, bool withAudio);

    // 接 glReadPixels 的结果：bottom-up 的 RGBA8，内部负责翻转和像素格式转换
    bool writeFrame(const uint8_t* rgba);

    // S16 交错立体声，采样率固定 44100
    bool writeAudio(const int16_t* samples, int frameCount);

    // 冲刷编码器里残留的帧并写完文件尾。析构时若没调过也会补一次
    bool close();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
