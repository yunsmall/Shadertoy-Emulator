#pragma once

#include <SFML/Audio.hpp>
#include <vector>
#include <queue>
#include <mutex>

// 自定义音频流，支持无限时长
class SoundShaderStream : public sf::SoundStream {
public:
    SoundShaderStream();
    ~SoundShaderStream() override;

    // 初始化流
    void init(int sampleRate = 44100);

    // 添加音频数据到队列
    void pushSamples(const std::vector<int16_t>& samples);

    // 获取当前播放位置（采样数）
    int64_t getCurrentSample() const { return m_currentSample; }

    // 获取就绪缓冲区数量
    size_t getReadyBufferCount() const;

private:
    bool onGetData(Chunk& data) override;
    void onSeek(sf::Time timeOffset) override;

    int m_sampleRate = 44100;
    int64_t m_currentSample = 0;

    // 缓冲队列
    mutable std::mutex m_mutex;
    std::queue<std::vector<int16_t>> m_bufferQueue;
    std::vector<int16_t> m_currentChunk;
    size_t m_currentChunkOffset = 0;
};
