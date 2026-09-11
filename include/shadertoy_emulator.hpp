#pragma once

#include "shader_config.hpp"
#include "gl_framebuffer.hpp"
#include "sound_stream.hpp"
#include "run_options.hpp"
#include "video_writer.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/Window/Context.hpp>
#include <SFML/Audio.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include <memory>
#include <string>
#include <array>
#include <chrono>
#include <vector>
#include <map>

class ShadertoyEmulator {
public:
    ShadertoyEmulator(const ShaderConfig& config, const RunOptions& options);
    void run();

private:
    // 渲染通道（运行时）
    struct RenderPass {
        std::string name;
        sf::Shader shader;
        std::unique_ptr<GLFramebuffer> framebuffer;      // 主 FBO（浮点纹理）
        std::unique_ptr<GLFramebuffer> framebufferAlt;   // 双缓冲备用
        std::array<std::optional<ChannelInput>, 4> channels;
        int width;
        int height;
        bool useWindowResolution = false;  // 是否使用窗口分辨率
        bool useDoubleBuffer = false;
        int currentBuffer = 0;
        bool isImage = false;
        bool isSound = false;

        // 编译后预查的 uniform location。-1 表示 shader 里没有这个 uniform
        // （GLSL 编译器会把没用到的优化掉），这时不能调 setUniform，否则 SFML 每帧刷一行警告
        GLint locIResolution = -1, locITime = -1, locITimeDelta = -1, locIFrame = -1;
        GLint locIFrameRate = -1, locIMouse = -1, locIDate = -1;
        GLint locChannelResolution = -1, locChannelTime = -1;
        GLint locISampleRate = -1, locISampleOffset = -1;
        std::array<GLint, 4> locChannels = {-1, -1, -1, -1};

        GLFramebuffer* getWriteTarget() {
            return useDoubleBuffer ? (currentBuffer == 0 ? framebuffer.get() : framebufferAlt.get())
                                   : framebuffer.get();
        }
        GLFramebuffer* getReadTarget() {
            return useDoubleBuffer ? (currentBuffer == 0 ? framebufferAlt.get() : framebuffer.get())
                                   : framebuffer.get();
        }
        void swapBuffer() {
            if (useDoubleBuffer) currentBuffer = 1 - currentBuffer;
        }
    };

    // 初始化
    void initPasses();
    bool loadShader(RenderPass& pass, const PassConfig& config);
    bool loadCommonCode();
    std::string wrapProcessedShader(const std::string& processedCode);
    std::string wrapSoundShader(const std::string& userCode);

    // 渲染
    void handleEvents();
    void beginFrame();  // 每帧开头算一次时间，保证所有 pass 看到同一份 iTime/iFrame
    void cacheUniformLocations(RenderPass& pass);  // 编译后查一次 location，之后按需设置
    void updateUniforms(RenderPass& pass, int width, int height);
    sf::Glsl::Vec4 mouseUniform() const;  // iMouse 的 z/w 靠符号位编码状态，所有 pass 必须用同一份
    void resizeFramebuffers();
    void renderPasses();
    void renderPass(RenderPass& pass);
    void bindChannels(RenderPass& pass);  // 绑定 4 个输入通道并上报 iChannelResolution
    void renderToScreen();
    void presentToWindow();  // 把输出目标贴到窗口上，离屏模式不调用

    // 纹理管理
    GLTexture* getChannelTexture(const ChannelInput& input);
    bool loadTextureFile(const ChannelInput& input);
    GLuint getSampler(ChannelInput::Filter filter, ChannelInput::Wrap wrap);

    // 键盘输入
    void initKeyboardTexture();
    void updateKeyboardTexture();
    int mapSfmlKeyToShadertoy(sf::Keyboard::Key key);
    int mapSfmlScancodeToShadertoy(sf::Keyboard::Scancode scancode);

    // 声音着色器
    void initSoundPass(RenderPass& pass);
    void generateSoundBatch();  // 按当前模式补足音频
    void renderSoundBatch(int batchSamples);  // 渲染一批采样并送去播放/编码/存档
    void pausePlayback();                     // 暂停画面与声音
    void resyncAudio(float targetTime);       // 让音频从画面时间 targetTime 处重新开始
    void checkAndGenerateSound();  // 检查并生成音频

    // ImGui
    void initImGui();
    void shutdownImGui();
    void renderImGui();
    void resetShader();

    // 三种运行模式的入口
    void runWindow();
    void runImages();
    void runVideo();

    // 导出辅助
    void captureFrame(int frameIndex);
    void readOutputPixels(std::vector<uint8_t>& pixels);  // 读输出目标，bottom-up RGBA8
    void writeAudioDump();
    void finishExport(const std::string& label, int renderedFrames,
                      std::chrono::steady_clock::time_point start);

    // 运行参数
    RunOptions m_options;

    // 窗口相关
    sf::RenderWindow m_window;
    int m_width;
    int m_height;

    // GUI 相关
    bool m_enableGui = false;
    bool m_paused = false;
    bool m_stepFrame = false;
    float m_pausedTime = 0.0f;
    float m_pausedTimeDelta = 0.0f;  // 暂停时保存的 iTimeDelta
    float m_currentFps = 0.0f;

    // 配置
    ShaderConfig m_config;
    std::string m_commonCode;

    // 音频导出：m_options.audioDumpPath 非空时把 Sound pass 的输出攒下来，跑完写成 WAV
    std::vector<int16_t> m_audioDumpSamples;

    // 视频模式：每帧渲染完把画面和这一帧对应的音频喂进去
    std::unique_ptr<VideoWriter> m_videoWriter;

    // 每帧时间，beginFrame() 算好后所有 pass 共用，避免 buffer 和 image 差一帧
    float m_frameTime = 0.0f;
    float m_frameTimeDelta = 1.0f / 60.0f;
    // 本帧帧号的快照，别删。m_frameCount 在 renderPasses() 末尾就自增了，而
    // renderToScreen() 和截图都发生在那之后，它们要的是"刚渲染的这一帧"的号；
    // 直接用 m_frameCount 会让 Image pass 的 iFrame 比 Buffer pass 多 1，导出文件名也整体偏移
    int m_frameIndex = 0;
    std::array<float, 4> m_frameDate = {2024.0f, 1.0f, 1.0f, 0.0f};

    std::unique_ptr<sf::Context> m_context;         // 离屏模式的 GL 上下文（无窗口）
    std::unique_ptr<GLFramebuffer> m_outputTarget;  // 所有 pass 的最终输出目标

    // 渲染通道
    std::vector<std::unique_ptr<RenderPass>> m_passes;
    std::map<std::string, RenderPass*> m_passMap;

    // 外部纹理缓存 (SFML 纹理用于文件加载，转换为 GLTexture)
    std::map<std::string, std::unique_ptr<GLTexture>> m_textureCache;

    // 采样器缓存，下标 = filter * 3 + wrap（两者各只有 3 种取值，直接查表，比 map 更快）
    // 采样参数挂在 sampler object 上，就不必每帧改纹理自身的状态，
    // 同一纹理也才能被不同通道以各自的 filter/wrap 采样
    std::array<GLuint, 9> m_samplerCache{};

    // 时间相关
    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    int m_frameCount;

    // 鼠标状态
    float m_mouseDownX = 0.0f, m_mouseDownY = 0.0f;   // 最后一次按下时的位置
    float m_mouseClickX = 0.0f, m_mouseClickY = 0.0f; // 最后一次点击位置
    bool m_mouseDown = false;     // 按钮是否按下
    bool m_mouseJustClicked = false; // 本帧是否刚点击

    // 键盘状态
    std::unique_ptr<GLTexture> m_keyboardTexture;
    std::array<bool, 256> m_keyPressed{};
    std::array<bool, 256> m_keyPressedPrev{};

    // 声音着色器
    static constexpr int SOUND_SAMPLE_RATE = 44100;
    static constexpr int SOUND_BATCH_SAMPLES = 22050;  // 每批样本数（0.5秒）

    std::unique_ptr<SoundShaderStream> m_soundStream;
    RenderPass* m_soundPass = nullptr;  // Sound pass 指针
    int64_t m_soundSamplePosition = 0;  // 当前生成到的采样位置
    bool m_audioResyncPending = false;  // 待重同步的音频（要碰 GL，不能在 ImGui 回调里做）
    float m_audioResyncTarget = 0.0f;   // 重同步的目标画面时间（秒）
    float m_pauseAnchor = 0.0f;         // 暂停那一刻的画面时间，用来判断中间有没有步进过
    int m_soundBatchSamples = SOUND_BATCH_SAMPLES;  // 实际批次宽度，受 GL_MAX_TEXTURE_SIZE 限制

    // 全屏四边形 VAO/VBO
    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    // 初始化 OpenGL 顶点数据
    void initQuad();
};
