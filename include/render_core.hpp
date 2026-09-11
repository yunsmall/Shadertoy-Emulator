#pragma once

#include "shader_config.hpp"
#include "gl_framebuffer.hpp"
#include "render_pass.hpp"
#include "texture_cache.hpp"
#include "run_options.hpp"
#include <SFML/Graphics.hpp>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// 一帧的时间参数。由播放侧算好——渲染只管用，不关心它是真实时钟还是虚拟时间
struct FrameState {
    float time = 0.0f;
    float timeDelta = 1.0f / 60.0f;
    int frameIndex = 0;
    std::array<float, 4> date = {2024.0f, 1.0f, 1.0f, 0.0f};
    sf::Glsl::Vec4 mouse;
};

// 生成音频采样要的时间参数。采样位置是独立于画面帧的另一条时间轴，喂不了 FrameState
struct SoundState {
    float time = 0.0f;         // 这一段采样的起始时刻
    int frame = 0;
    int64_t sampleOffset = 0;
    sf::Glsl::Vec4 mouse;
};

// 所有 pass 的编译、FBO、纹理和每帧的绘制都在这儿。它不认识窗口，也不认识 ImGui：
// 给一份 FrameState 就画一帧，窗口和离屏两种模式共用同一套
class RenderCore {
public:
    RenderCore(const ShaderConfig& config, const RunOptions& options);

    // 建 VAO、输出目标和全部 pass。必须在 GL 上下文就绪之后调用
    void init(int width, int height);
    // 窗口尺寸变了：输出目标和跟着窗口走的 buffer 一起重建
    void resize(int width, int height);

    // 按键状态上传到 iKeyboard 纹理。Shader 靠它认键盘
    void updateKeyboard(const std::array<bool, 256>& keys);

    // 渲染 buffer pass，按 config 里的顺序走一遍。intermediate=true 是跳帧模式的
    // 中间帧：只渲染状态链条上的通道（见 computeMustRunPasses），无状态的直接跳过
    void renderBufferPasses(const FrameState& frame, bool intermediate = false);

    // 把画面贴到输出目标：debugName 指定的 pass 顶替 Image，没开就正常跑 Image
    void renderToScreen(const FrameState& frame, const std::string& debugName);

    // 渲染一批音频采样。生成归这里（它本来就是渲染），送去播放还是存盘由调用方定
    void renderSoundBatch(int batchSamples, const SoundState& state, std::vector<int16_t>& out);

    RenderPass* findPass(const std::string& name);  // 找不到返回 nullptr
    RenderPass* imagePass();
    // 要在画面上顶替 Image 的 pass。名字为空或对不上就是 nullptr
    RenderPass* debugViewPass(const std::string& wantName);
    // 画面上此刻显示的是哪个 FBO：像素探针按它取原始值，调试视图同理
    GLFramebuffer* currentViewTarget(const std::string& debugName);

    GLFramebuffer* outputTarget() { return m_outputTarget.get(); }
    const std::vector<std::unique_ptr<RenderPass>>& passes() const { return m_passes; }
    // 跳帧模式的中间帧也得渲染的通道，依赖分析的结果。调用方拿它告诉使用者跳了谁
    const std::set<RenderPass*>& mustRunEveryFrame() const { return m_mustRunEveryFrame; }

    // 把 pass 的内容缩进一张小纹理给 ImGui 用。走 GPU blit 而不是读回 CPU：
    // 读回会把管线卡住，一帧读好几个 buffer 很伤。尺寸对不上会重建目标
    void blitToThumbnail(RenderPass& pass);
    std::map<std::string, std::unique_ptr<GLFramebuffer>>& thumbnails() { return m_thumbnails; }

    bool hasSoundPass() const { return m_soundPass != nullptr; }
    int soundBatchSamples() const { return m_soundBatchSamples; }

private:
    // 初始化
    void initQuad();
    bool loadCommonCode();
    void initPasses();
    bool loadShader(RenderPass& pass, const PassConfig& config);
    void cacheUniformLocations(RenderPass& pass);
    // isImage 决定输出怎么写：Image 通道是屏幕，alpha 钉成 1；Buffer 通道的 alpha
    // 是数据，原样透传（Shadertoy 的 image 和 buffer 也是两套不同的 footer）
    std::string wrapProcessedShader(const std::string& processedCode, bool isImage);
    std::string wrapSoundShader(const std::string& processedCode);
    bool initSoundPass(RenderPass& pass);  // 编译不过返回 false，调用方当它不存在
    void initKeyboardTexture();
    // 跳帧模式的中间帧该渲染哪些通道：输出传递依赖自己的（自引用、引用环）必须每帧走，
    // 它们读到的通道也得跟着走。结果存进 m_mustRunEveryFrame
    void computeMustRunPasses();

    // 渲染
    void updateUniforms(RenderPass& pass, int width, int height, const FrameState& frame);
    void bindChannels(RenderPass& pass);
    GLTexture* getChannelTexture(const ChannelInput& input);
    void renderPass(RenderPass& pass, const FrameState& frame);

    const ShaderConfig& m_config;
    const RunOptions& m_options;

    int m_width = 0;
    int m_height = 0;
    std::string m_commonCode;

    // 全屏四边形
    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    std::unique_ptr<GLFramebuffer> m_outputTarget;
    std::vector<std::unique_ptr<RenderPass>> m_passes;
    std::map<std::string, RenderPass*> m_passMap;
    // 跳帧模式的中间帧必须渲染的通道，computeMustRunPasses() 算出来的
    std::set<RenderPass*> m_mustRunEveryFrame;
    TextureCache m_textures;

    // 键盘纹理。上一帧的按键状态只在这儿用，属于渲染状态而非输入状态
    std::unique_ptr<GLTexture> m_keyboardTexture;
    std::array<bool, 256> m_keyPressedPrev{};

    // 缩略图：勾上才刷新，平时一点 GPU 开销都不花。每个 buffer 一张自己的 FBO。
    // ImGui-SFML 的 ImTextureID 就是 GL 纹理名，直接把纹理递过去就行，不必绕道 sf::Texture
    std::map<std::string, std::unique_ptr<GLFramebuffer>> m_thumbnails;

    RenderPass* m_soundPass = nullptr;
    int m_soundBatchSamples = 22050;  // 一批多少采样，受 GL_MAX_TEXTURE_SIZE 限制
    std::vector<float> m_soundFloatData;  // 读回采样的中转，得按 FBO 宽度开
};
