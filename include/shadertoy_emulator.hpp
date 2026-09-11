#pragma once

#include "shader_config.hpp"
#include "input_state.hpp"
#include "render_core.hpp"
#include "sound_stream.hpp"
#include "run_options.hpp"
#include "exporter.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/Window/Context.hpp>
#include <SFML/Audio.hpp>
#include <memory>
#include <string>
#include <array>
#include <chrono>
#include <vector>

class DebugOverlay;  // 定义在 debug_overlay.hpp

// 串起整个程序：窗口/离屏上下文、播放控制、声音播放、调试面板和三种运行模式。
// 真正干活的在 RenderCore（渲染）、Exporter（落盘）和 InputState（输入）里
class ShadertoyEmulator {
public:
    ShadertoyEmulator(const ShaderConfig& config, const RunOptions& options);
    ~ShadertoyEmulator();  // 定义在 cpp 里：overlay 的类型在这里还不完整
    void run();

private:
    // 调试面板要直接读写这里的状态。它就是这个程序的视图，中间隔一层接口
    // 只会得到一堆转发函数
    friend class DebugOverlay;
    // 每帧
    void handleEvents();
    void beginFrame();  // 每帧开头算一次时间，保证所有 pass 看到同一份 iTime/iFrame
    void renderPasses();
    void renderToScreen();
    void presentToWindow();  // 把输出目标贴到窗口上，离屏模式不调用

    // 声音。采样怎么算是 RenderCore 的事，这里只管什么时候生成、送去哪
    void generateSoundBatch();                // 按当前模式补足音频
    void renderSoundBatch(int batchSamples);  // 生成一批采样并送去播放/编码/存档
    void pausePlayback();                     // 暂停画面与声音
    void resyncAudio(float targetTime);       // 让音频从画面时间 targetTime 处重新开始
    void checkAndGenerateSound();

    void resetShader();

    // 三种运行模式的入口
    void runWindow();
    void runImages();
    void runVideo();

    // 运行参数
    RunOptions m_options;

    // 窗口相关
    sf::RenderWindow m_window;
    int m_width;
    int m_height;

    // 离屏模式的 GL 上下文（无窗口）。它和 m_window 谁在，GL 资源就跟着谁活，
    // 所以下面所有持有 GL 对象的东西都必须声明在它们后面，才能保证析构时 GL 还在
    std::unique_ptr<sf::Context> m_context;

    // 配置
    ShaderConfig m_config;

    // 渲染：pass 的编译、FBO、纹理、每帧绘制
    RenderCore m_core;

    // 落盘：截图、buffer dump、WAV、视频
    Exporter m_exporter;

    // 鼠标与键盘
    InputState m_input;

    // GUI 相关
    bool m_enableGui = false;
    std::unique_ptr<DebugOverlay> m_overlay;
    // 调试视图：非空时画面显示这个 pass 的 buffer，而不是 Image pass。
    // 存名字不存指针——resetShader() 会重建所有 pass，原来的指针会悬空
    std::string m_debugViewPass;
    bool m_paused = false;
    bool m_stepFrame = false;
    float m_pausedTime = 0.0f;
    float m_pausedTimeDelta = 0.0f;  // 暂停时保存的 iTimeDelta
    float m_currentFps = 0.0f;

    // 本帧的时间快照，所有 pass 共用，避免 buffer 和 image 差一帧
    FrameState m_frame;

    // 缩略图默认关着：每个 buffer 每帧都要 blit 一次，不看时白花这份开销
    bool m_showThumbnails = false;

    // 像素探针。glReadPixels 是同步的，每帧读一次要等 GPU 画完（实测重负载
    // shader 上要 7~12ms），所以鼠标不动时降频读，值缓存在这里
    bool m_showPixelProbe = true;
    std::array<float, 4> m_probePixel = {0.0f, 0.0f, 0.0f, 0.0f};
    float m_probeLastX = -1.0f;
    float m_probeLastY = -1.0f;
    int m_probeIdleFrames = 0;

    // 时间相关
    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    int m_frameCount;

    // 声音播放。采样位置是独立于画面帧的另一条时间轴
    std::unique_ptr<SoundShaderStream> m_soundStream;
    std::vector<int16_t> m_audioSamples;  // 每批采样的中转，复用免得反复分配
    int64_t m_soundSamplePosition = 0;    // 当前生成到的采样位置
    bool m_audioResyncPending = false;  // 待重同步的音频（要碰 GL，不能在 ImGui 回调里做）
    float m_audioResyncTarget = 0.0f;   // 重同步的目标画面时间（秒）
    float m_pauseAnchor = 0.0f;         // 暂停那一刻的画面时间，用来判断中间有没有步进过
};
