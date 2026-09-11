#include "exporter.hpp"
#include "audio_format.hpp"
#include "render_pass.hpp"
#include "video_writer.hpp"
#include <SFML/Graphics.hpp>
#include <glad/glad.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

Exporter::Exporter(const RunOptions& options) : m_options(options) {}

Exporter::~Exporter() = default;

void Exporter::setOutputTarget(GLFramebuffer* target, int width, int height) {
    m_outputTarget = target;
    m_width = width;
    m_height = height;
}

bool Exporter::beginVideo(bool hasAudio) {
    const int fps = static_cast<int>(m_options.fps + 0.5f);

    m_videoWriter = std::make_unique<VideoWriter>();
    if (!m_videoWriter->open(m_options.videoPath, m_width, m_height, fps, hasAudio)) {
        std::cerr << "Failed to open video output: " << m_options.videoPath << std::endl;
        m_videoWriter.reset();
        return false;
    }
    return true;
}

void Exporter::writeVideoFrame() {
    if (!m_videoWriter) return;

    readOutputPixels(m_pixelBuffer);
    m_videoWriter->writeFrame(m_pixelBuffer.data());
}

void Exporter::endVideo() {
    if (!m_videoWriter) return;

    m_videoWriter->close();
    m_videoWriter.reset();
}

void Exporter::readOutputPixels(std::vector<uint8_t>& pixels) {
    pixels.resize(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_outputTarget->fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Exporter::captureFrame(int frameIndex) {
    if (!m_outputTarget || m_outputTarget->fbo == 0 || m_width <= 0 || m_height <= 0) return;

    readOutputPixels(m_pixelBuffer);

    // glReadPixels 是 bottom-up，sf::Image 期望 top-down
    sf::Image image(sf::Vector2u(static_cast<unsigned>(m_width), static_cast<unsigned>(m_height)),
                    m_pixelBuffer.data());
    image.flipVertically();

    std::ostringstream name;
    name << std::setfill('0') << std::setw(5) << frameIndex << ".png";

    std::filesystem::path outPath = m_options.imageDir / name.str();
    if (!image.saveToFile(outPath)) {
        std::cerr << "Failed to save " << outPath << std::endl;
    }
}

// buffer 里存的是浮点，直接当颜色存会把负值和超过 1 的部分全削掉，所以先乘一个
// 增益再 clamp——想看清暗部的中间值时把增益调大
void Exporter::captureBuffer(RenderPass& pass, int frameIndex) {
    GLFramebuffer* target = pass.getReadTarget();
    const int w = target->colorTex.width;
    const int h = target->colorTex.height;
    if (w <= 0 || h <= 0) return;

    std::vector<float> raw(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, target->fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, raw.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::vector<uint8_t> pixels(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        const float v = std::clamp(raw[i] * m_options.dumpBufferGain, 0.0f, 1.0f);
        pixels[i] = static_cast<uint8_t>(v * 255.0f + 0.5f);
    }

    sf::Image image(sf::Vector2u(static_cast<unsigned>(w), static_cast<unsigned>(h)), pixels.data());
    image.flipVertically();

    std::ostringstream name;
    name << std::setfill('0') << std::setw(5) << frameIndex << ".png";

    // 放子目录：和主图平级的话，按 *.png 数张数的脚本会多数出一堆
    const std::filesystem::path outPath =
        m_options.imageDir / "buffers" / pass.name / name.str();
    std::error_code ec;
    std::filesystem::create_directories(outPath.parent_path(), ec);
    if (!image.saveToFile(outPath)) {
        std::cerr << "Failed to save " << outPath << std::endl;
    }
}

void Exporter::appendAudio(const std::vector<int16_t>& samples) {
    if (m_videoWriter) {
        // 编码器按帧算：交错立体声两个 int16 才是一帧
        m_videoWriter->writeAudio(samples.data(), samples.size() / 2);
    }
    if (!m_options.audioDumpPath.empty()) {
        m_audioDumpSamples.insert(m_audioDumpSamples.end(), samples.begin(), samples.end());
    }
}

void Exporter::writeAudioDump() {
    if (m_options.audioDumpPath.empty()) return;

    if (m_audioDumpSamples.empty()) {
        std::cerr << "No audio to dump (config has no Sound pass?)" << std::endl;
        return;
    }

    std::ofstream out(m_options.audioDumpPath, std::ios::binary);
    if (!out) {
        std::cerr << "Cannot open audio dump file: " << m_options.audioDumpPath << std::endl;
        return;
    }

    const uint32_t dataSize = static_cast<uint32_t>(m_audioDumpSamples.size() * sizeof(int16_t));
    const uint32_t sampleRate = SOUND_SAMPLE_RATE;
    const uint16_t channels = 2;
    const uint16_t bitsPerSample = 16;
    const uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
    const uint16_t blockAlign = channels * bitsPerSample / 8;

    auto write = [&out](const void* data, std::streamsize size) {
        out.write(static_cast<const char*>(data), size);
    };

    uint32_t chunkSize = 36 + dataSize;
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;  // PCM

    write("RIFF", 4);
    write(&chunkSize, 4);
    write("WAVE", 4);
    write("fmt ", 4);
    write(&fmtSize, 4);
    write(&audioFormat, 2);
    write(&channels, 2);
    write(&sampleRate, 4);
    write(&byteRate, 4);
    write(&blockAlign, 2);
    write(&bitsPerSample, 2);
    write("data", 4);
    write(&dataSize, 4);
    write(m_audioDumpSamples.data(), static_cast<std::streamsize>(dataSize));

    const size_t frames = m_audioDumpSamples.size() / 2;
    std::cout << "Wrote audio dump: " << m_options.audioDumpPath << " (" << frames << " frames, "
              << (static_cast<double>(frames) / SOUND_SAMPLE_RATE) << "s)" << std::endl;
}

void Exporter::finish(const std::string& label, int renderedFrames,
                      std::chrono::steady_clock::time_point start) {
    writeAudioDump();

    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "Exported " << label << " in " << std::fixed << std::setprecision(2)
              << elapsed << "s";
    if (renderedFrames > 0) {
        std::cout << " (" << (elapsed * 1000.0 / renderedFrames) << " ms/frame)";
    }
    std::cout << std::endl;
}
