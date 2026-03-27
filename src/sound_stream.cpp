#include "sound_stream.hpp"

SoundShaderStream::SoundShaderStream() = default;
SoundShaderStream::~SoundShaderStream() = default;

void SoundShaderStream::init(int sampleRate) {
    m_sampleRate = sampleRate;
    // 立体声，采样率，通道映射
    initialize(2, sampleRate, {sf::SoundChannel::FrontLeft, sf::SoundChannel::FrontRight});
}

void SoundShaderStream::pushSamples(const std::vector<int16_t>& samples) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bufferQueue.push(samples);
}

size_t SoundShaderStream::getReadyBufferCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bufferQueue.size();
}

bool SoundShaderStream::onGetData(Chunk& data) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 从队列获取数据
    if (m_bufferQueue.empty()) {
        // 没有数据，返回静音（大小与正常批次一致）
        static std::vector<int16_t> silence(44100, 0);  // 1秒静音
        data.samples = silence.data();
        data.sampleCount = silence.size() / 2;
        return true;
    }

    // 取出一个缓冲区
    m_currentChunk = std::move(m_bufferQueue.front());
    m_bufferQueue.pop();

    data.samples = m_currentChunk.data();
    data.sampleCount = m_currentChunk.size() / 2;
    m_currentSample += data.sampleCount;

    return true;
}

void SoundShaderStream::onSeek(sf::Time timeOffset) {
    // 不支持 seek，忽略
    (void)timeOffset;
}
