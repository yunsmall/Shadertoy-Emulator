#pragma once

#include "shader_config.hpp"
#include "gl_framebuffer.hpp"
#include "sound_stream.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/Audio.hpp>
#include <memory>
#include <string>
#include <array>
#include <chrono>
#include <vector>
#include <map>

class ShadertoyEmulator {
public:
    explicit ShadertoyEmulator(const ShaderConfig& config, bool showFps = false);
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
    void updateUniforms(sf::Shader& shader, int width, int height);
    void resizeFramebuffers();
    void renderPasses();
    void renderPass(RenderPass& pass);
    void renderToScreen();

    // 纹理管理
    const GLTexture* getChannelTexture(const ChannelInput& input);
    bool loadTextureFile(const ChannelInput& input);

    // 键盘输入
    void initKeyboardTexture();
    void updateKeyboardTexture();
    int mapSfmlKeyToShadertoy(sf::Keyboard::Key key);
    int mapSfmlScancodeToShadertoy(sf::Keyboard::Scancode scancode);

    // 声音着色器
    void initSoundPass(RenderPass& pass);
    void generateSoundBatch();  // 生成一批音频
    void checkAndGenerateSound();  // 检查并生成音频

    // 窗口相关
    sf::RenderWindow m_window;
    int m_width;
    int m_height;
    bool m_showFps;

    // 配置
    ShaderConfig m_config;
    std::string m_commonCode;

    // 渲染通道
    std::vector<std::unique_ptr<RenderPass>> m_passes;
    std::map<std::string, RenderPass*> m_passMap;

    // 外部纹理缓存 (SFML 纹理用于文件加载，转换为 GLTexture)
    std::map<std::string, std::unique_ptr<GLTexture>> m_textureCache;

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
