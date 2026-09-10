#include "video_writer.hpp"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace {

constexpr int AUDIO_SAMPLE_RATE = 44100;

std::string avError(int err) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(err, buf, sizeof(buf));
    return buf;
}

}  // namespace

struct VideoWriter::Impl {
    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* videoCtx = nullptr;
    AVCodecContext* audioCtx = nullptr;
    AVStream* videoStream = nullptr;
    AVStream* audioStream = nullptr;
    SwsContext* swsCtx = nullptr;
    SwrContext* swrCtx = nullptr;
    AVFrame* videoFrame = nullptr;
    AVFrame* audioFrame = nullptr;
    AVPacket* packet = nullptr;

    int width = 0;
    int height = 0;
    int fps = 1;
    int64_t videoPts = 0;
    int64_t audioPts = 0;

    std::vector<uint8_t> flipBuf;      // glReadPixels 是 bottom-up，翻转用
    std::vector<int16_t> audioPending; // 不足一个 AAC 帧的样本先攒着
    bool audioDone = false;

    // 收包并写出。编码器内部有缓冲，所以每次 send 之后都要把能收的包全部收完
    bool drain(AVCodecContext* ctx, AVStream* stream) {
        while (true) {
            int ret = avcodec_receive_packet(ctx, packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return true;
            if (ret < 0) {
                std::cerr << "VideoWriter: receive_packet failed: " << avError(ret) << std::endl;
                return false;
            }
            av_packet_rescale_ts(packet, ctx->time_base, stream->time_base);
            packet->stream_index = stream->index;
            ret = av_interleaved_write_frame(formatCtx, packet);
            av_packet_unref(packet);
            if (ret < 0) {
                std::cerr << "VideoWriter: write_frame failed: " << avError(ret) << std::endl;
                return false;
            }
        }
    }

    // 把攒够的音频编成 AAC 帧。输入是 S16 交错立体声，编码器要 FLTP planar
    bool encodePendingAudio() {
        const int frameSize = audioCtx->frame_size;
        while (static_cast<int>(audioPending.size() / 2) >= frameSize) {
            if (av_frame_make_writable(audioFrame) < 0) return false;

            const uint8_t* inPtr = reinterpret_cast<const uint8_t*>(audioPending.data());
            int converted = swr_convert(swrCtx, audioFrame->data, frameSize, &inPtr, frameSize);
            if (converted < 0) {
                std::cerr << "VideoWriter: swr_convert failed: " << avError(converted) << std::endl;
                return false;
            }
            audioPending.erase(audioPending.begin(), audioPending.begin() + converted * 2);

            // 转换没填满一帧时补静音，编码器不接受半帧
            if (converted < frameSize) {
                for (int ch = 0; ch < 2; ++ch) {
                    float* dst = reinterpret_cast<float*>(audioFrame->data[ch]);
                    std::memset(dst + converted, 0, sizeof(float) * (frameSize - converted));
                }
            }

            audioFrame->nb_samples = frameSize;
            audioFrame->pts = audioPts;
            audioPts += frameSize;

            if (avcodec_send_frame(audioCtx, audioFrame) < 0) return false;
            if (!drain(audioCtx, audioStream)) return false;
        }
        return true;
    }
};

VideoWriter::VideoWriter() : m_impl(std::make_unique<Impl>()) {}

VideoWriter::~VideoWriter() {
    close();
}

bool VideoWriter::open(const std::filesystem::path& path, int width, int height, int fps,
                       bool withAudio) {
    if (m_impl->formatCtx != nullptr) return false;
    if (width <= 0 || height <= 0 || fps <= 0) return false;

    Impl& impl = *m_impl;

    // H.264 用的 YUV420P 要求宽高都是偶数。这里不能偷偷对齐：调用方按自己的尺寸
    // 读像素，尺寸对不上整帧就错位了
    if ((width & 1) != 0 || (height & 1) != 0) {
        std::cerr << "VideoWriter: width and height must be even (got " << width << "x" << height
                  << ")" << std::endl;
        return false;
    }
    impl.width = width;
    impl.height = height;
    impl.fps = fps;

    int ret = avformat_alloc_output_context2(&impl.formatCtx, nullptr, nullptr,
                                             path.string().c_str());
    if (ret < 0 || impl.formatCtx == nullptr) {
        std::cerr << "VideoWriter: cannot create output context: " << avError(ret) << std::endl;
        return false;
    }

    // ---- 视频流 ----
    const AVCodec* videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (videoCodec == nullptr) {
        std::cerr << "VideoWriter: H.264 encoder not available (ffmpeg built without libx264?)"
                  << std::endl;
        return false;
    }
    impl.videoCtx = avcodec_alloc_context3(videoCodec);
    if (impl.videoCtx == nullptr) return false;

    impl.videoCtx->width = impl.width;
    impl.videoCtx->height = impl.height;
    impl.videoCtx->time_base = {1, fps};
    impl.videoCtx->framerate = {fps, 1};
    impl.videoCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    // 按 0.125 bit/像素/帧估，720p60 约 7 Mbps，够保住 shader 那种大面积渐变
    impl.videoCtx->bit_rate = static_cast<int64_t>(impl.width) * impl.height * fps / 8;

    impl.videoStream = avformat_new_stream(impl.formatCtx, nullptr);
    if (impl.videoStream == nullptr) return false;
    impl.videoStream->time_base = impl.videoCtx->time_base;

    if (avcodec_open2(impl.videoCtx, videoCodec, nullptr) < 0) {
        std::cerr << "VideoWriter: cannot open H.264 encoder" << std::endl;
        return false;
    }
    if (avcodec_parameters_from_context(impl.videoStream->codecpar, impl.videoCtx) < 0) return false;

    impl.videoFrame = av_frame_alloc();
    if (impl.videoFrame == nullptr) return false;
    impl.videoFrame->format = impl.videoCtx->pix_fmt;
    impl.videoFrame->width = impl.videoCtx->width;
    impl.videoFrame->height = impl.videoCtx->height;
    if (av_frame_get_buffer(impl.videoFrame, 0) < 0) return false;

    impl.flipBuf.resize(static_cast<size_t>(impl.width) * impl.height * 4);

    impl.swsCtx = sws_getContext(impl.width, impl.height, AV_PIX_FMT_RGBA,
                                 impl.width, impl.height, AV_PIX_FMT_YUV420P,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (impl.swsCtx == nullptr) {
        std::cerr << "VideoWriter: cannot create scaling context" << std::endl;
        return false;
    }

    // ---- 音频流 ----
    if (withAudio) {
        const AVCodec* audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (audioCodec == nullptr) {
            std::cerr << "VideoWriter: AAC encoder not available, writing video only" << std::endl;
        } else {
            impl.audioCtx = avcodec_alloc_context3(audioCodec);
            if (impl.audioCtx == nullptr) return false;

            impl.audioCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
            impl.audioCtx->sample_rate = AUDIO_SAMPLE_RATE;
            impl.audioCtx->bit_rate = 192000;
            av_channel_layout_default(&impl.audioCtx->ch_layout, 2);
            impl.audioCtx->time_base = {1, AUDIO_SAMPLE_RATE};

            impl.audioStream = avformat_new_stream(impl.formatCtx, nullptr);
            if (impl.audioStream == nullptr) return false;
            impl.audioStream->time_base = impl.audioCtx->time_base;

            if (avcodec_open2(impl.audioCtx, audioCodec, nullptr) < 0) {
                std::cerr << "VideoWriter: cannot open AAC encoder, writing video only" << std::endl;
                avcodec_free_context(&impl.audioCtx);
            } else {
                if (avcodec_parameters_from_context(impl.audioStream->codecpar, impl.audioCtx) < 0) {
                    return false;
                }

                impl.audioFrame = av_frame_alloc();
                if (impl.audioFrame == nullptr) return false;
                impl.audioFrame->format = impl.audioCtx->sample_fmt;
                impl.audioFrame->sample_rate = impl.audioCtx->sample_rate;
                impl.audioFrame->nb_samples = impl.audioCtx->frame_size;
                if (av_channel_layout_copy(&impl.audioFrame->ch_layout,
                                           &impl.audioCtx->ch_layout) < 0) {
                    return false;
                }
                if (av_frame_get_buffer(impl.audioFrame, 0) < 0) return false;

                // 输入是 S16 交错立体声，和输出的采样率一致，只做格式转换
                AVChannelLayout stereo;
                av_channel_layout_default(&stereo, 2);
                if (swr_alloc_set_opts2(&impl.swrCtx, &impl.audioCtx->ch_layout,
                                        impl.audioCtx->sample_fmt, AUDIO_SAMPLE_RATE,
                                        &stereo, AV_SAMPLE_FMT_S16, AUDIO_SAMPLE_RATE,
                                        0, nullptr) < 0 ||
                    swr_init(impl.swrCtx) < 0) {
                    std::cerr << "VideoWriter: cannot create resampler, writing video only"
                              << std::endl;
                    avcodec_free_context(&impl.audioCtx);
                    impl.audioStream = nullptr;
                }
            }
        }
    }

    impl.packet = av_packet_alloc();
    if (impl.packet == nullptr) return false;

    if ((impl.formatCtx->oformat->flags & AVFMT_NOFILE) == 0) {
        ret = avio_open(&impl.formatCtx->pb, path.string().c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            std::cerr << "VideoWriter: cannot open " << path << ": " << avError(ret) << std::endl;
            return false;
        }
    }

    ret = avformat_write_header(impl.formatCtx, nullptr);
    if (ret < 0) {
        std::cerr << "VideoWriter: cannot write header: " << avError(ret) << std::endl;
        return false;
    }

    return true;
}

bool VideoWriter::writeFrame(const uint8_t* rgba) {
    Impl& impl = *m_impl;
    if (impl.videoCtx == nullptr || rgba == nullptr) return false;

    if (av_frame_make_writable(impl.videoFrame) < 0) return false;

    // 逐行倒着拷一遍：glReadPixels 给的是 bottom-up，编码器要 top-down
    const size_t rowBytes = static_cast<size_t>(impl.width) * 4;
    for (int y = 0; y < impl.height; ++y) {
        std::memcpy(impl.flipBuf.data() + static_cast<size_t>(y) * rowBytes,
                    rgba + static_cast<size_t>(impl.height - 1 - y) * rowBytes, rowBytes);
    }

    const uint8_t* srcData[4] = {impl.flipBuf.data(), nullptr, nullptr, nullptr};
    const int srcStride[4] = {static_cast<int>(rowBytes), 0, 0, 0};
    sws_scale(impl.swsCtx, srcData, srcStride, 0, impl.height,
              impl.videoFrame->data, impl.videoFrame->linesize);

    impl.videoFrame->pts = impl.videoPts++;

    if (avcodec_send_frame(impl.videoCtx, impl.videoFrame) < 0) return false;
    return impl.drain(impl.videoCtx, impl.videoStream);
}

bool VideoWriter::writeAudio(const int16_t* samples, int frameCount) {
    Impl& impl = *m_impl;
    if (impl.audioCtx == nullptr || samples == nullptr || frameCount <= 0) return true;  // 没音轨就跳过

    impl.audioPending.insert(impl.audioPending.end(), samples, samples + frameCount * 2);
    return impl.encodePendingAudio();
}

bool VideoWriter::close() {
    Impl& impl = *m_impl;
    if (impl.formatCtx == nullptr) return true;

    // 先冲刷两个编码器：它们内部还压着几帧，不 drain 出来会丢结尾
    if (impl.videoCtx != nullptr) {
        avcodec_send_frame(impl.videoCtx, nullptr);
        impl.drain(impl.videoCtx, impl.videoStream);
    }

    if (impl.audioCtx != nullptr && !impl.audioDone) {
        impl.audioDone = true;
        if (!impl.audioPending.empty()) {
            // 剩下的样本不够一个 AAC 帧，补静音凑满再编，否则结尾会被吃掉最多 23ms
            const int frameSize = impl.audioCtx->frame_size;
            impl.audioPending.resize(static_cast<size_t>(frameSize) * 2, 0);
            if (av_frame_make_writable(impl.audioFrame) >= 0) {
                const uint8_t* inPtr = reinterpret_cast<const uint8_t*>(impl.audioPending.data());
                if (swr_convert(impl.swrCtx, impl.audioFrame->data, frameSize, &inPtr, frameSize) >= 0) {
                    impl.audioFrame->nb_samples = frameSize;
                    impl.audioFrame->pts = impl.audioPts;
                    impl.audioPts += frameSize;
                    if (avcodec_send_frame(impl.audioCtx, impl.audioFrame) >= 0) {
                        impl.drain(impl.audioCtx, impl.audioStream);
                    }
                }
            }
        }
        avcodec_send_frame(impl.audioCtx, nullptr);
        impl.drain(impl.audioCtx, impl.audioStream);
    }

    bool ok = true;
    if (av_write_trailer(impl.formatCtx) < 0) {
        std::cerr << "VideoWriter: failed to write trailer" << std::endl;
        ok = false;
    }

    if (impl.swsCtx != nullptr) sws_freeContext(impl.swsCtx);
    if (impl.swrCtx != nullptr) swr_free(&impl.swrCtx);
    av_frame_free(&impl.videoFrame);
    av_frame_free(&impl.audioFrame);
    av_packet_free(&impl.packet);
    avcodec_free_context(&impl.videoCtx);
    avcodec_free_context(&impl.audioCtx);
    if (impl.formatCtx != nullptr) {
        if (impl.formatCtx->pb != nullptr) avio_closep(&impl.formatCtx->pb);
        avformat_free_context(impl.formatCtx);
    }

    impl.formatCtx = nullptr;
    impl.videoStream = nullptr;
    impl.audioStream = nullptr;
    return ok;
}
