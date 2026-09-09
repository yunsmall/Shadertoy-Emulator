#pragma once

#include "shader_config.hpp"
#include "gl_framebuffer.hpp"
#include "sound_stream.hpp"
#include "frame_range.hpp"
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
    explicit ShadertoyEmulator(const ShaderConfig& config, bool showFps = false, bool enableGui = false,
                               bool offscreen = false, const FrameRange& captureRange = {},
                               const std::filesystem::path& captureDir = {}, float offlineFps = 60.0f,
                               const std::filesystem::path& audioDumpPath = {});
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
    std::string wrapShader(const std::string& userCode);
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
    void generateSoundBatch();  // 生成一批音频
    void checkAndGenerateSound();  // 检查并生成音频

    // ImGui
    void initImGui();
    void shutdownImGui();
    void renderImGui();
    void resetShader();

    // 帧序列导出
    void runOffscreen();
    void captureFrame(int frameIndex);
    void writeAudioDump();

    // 窗口相关
    sf::RenderWindow m_window;
    int m_width;
    int m_height;
    bool m_showFps;

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

    // 离屏渲染与帧序列导出
    bool m_offscreen = false;
    FrameRange m_captureRange;
    std::filesystem::path m_captureDir;
    float m_offlineFps = 60.0f;  // 离屏模式的虚拟帧率，否则没有 vsync 时 iTime 几乎不涨

    // 音频导出：路径非空时把 Sound pass 的输出攒下来，跑完写成一个 WAV
    std::filesystem::path m_audioDumpPath;
    std::vector<int16_t> m_audioDumpSamples;

    // 每帧时间，beginFrame() 算好后所有 pass 共用，避免 buffer 和 image 差一帧
    float m_frameTime = 0.0f;
    float m_frameTimeDelta = 1.0f / 60.0f;
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

    // 全屏四边形 VAO/VBO
    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    // 初始化 OpenGL 顶点数据
    void initQuad();
};
