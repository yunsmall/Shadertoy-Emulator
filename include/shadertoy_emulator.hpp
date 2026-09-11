#pragma once

#include "shader_config.hpp"
#include "gl_framebuffer.hpp"
#include "render_pass.hpp"
#include "sound_stream.hpp"
#include "texture_cache.hpp"
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

    // 调试视图
    RenderPass* findPass(const std::string& name);  // 找不到返回 nullptr
    RenderPass* debugViewPass();        // 当前要在画面上顶替 Image 的 pass，没开或名字无效就是 nullptr
    GLFramebuffer* currentViewTarget(); // 画面上此刻显示的是哪个 FBO，像素探针按它取原始值
    void updateThumbnail(RenderPass& pass);

    // 三种运行模式的入口
    void runWindow();
    void runImages();
    void runVideo();

    // 导出辅助
    void captureFrame(int frameIndex);
    void captureBuffer(RenderPass& pass, int frameIndex);
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
    // 调试视图：非空时画面显示这个 pass 的 buffer，而不是 Image pass。
    // 存名字不存指针——resetShader() 会重建所有 pass，原来的指针会悬空
    std::string m_debugViewPass;
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

    // 文件纹理与采样器缓存
    TextureCache m_textures;

    // 缩略图：勾上才刷新，平时一点 GPU 开销都不花。
    // 每个 buffer 一张自己的 FBO。ImGui-SFML 的 ImTextureID 就是 GL 纹理名，
    // 直接把纹理递过去就行，不必绕道 sf::Texture。
    // GL 资源声明在窗口/上下文后面，才能保证析构时 GL 还活着
    bool m_showThumbnails = false;
    std::map<std::string, std::unique_ptr<GLFramebuffer>> m_thumbnails;

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
