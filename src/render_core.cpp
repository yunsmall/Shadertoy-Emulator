#include "render_core.hpp"
#include "glsl_preprocessor.hpp"
#include "glsl_translator.hpp"
#include "audio_format.hpp"
#include <glad/glad.h>
#include <fstream>
#include <filesystem>
#include <regex>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>

// 全屏四边形顶点数据
static const float QUAD_VERTICES[] = {
    // x, y
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f
};

// glslangValidator 展开 #include 后会写出 #line <n> "<file>"。带文件名那种写法是
// ARB_shading_language_include 的语法，标准 GLSL 只认 #line <n>，没实现该扩展的
// 驱动（比如 Mesa）会直接报语法错误，所以在这里统一把文件名丢掉
static std::string stripLineFilenames(const std::string& code) {
    static const std::regex re(R"(#line([ \t]+[0-9]+)[ \t]+"[^"]*")");
    return std::regex_replace(code, re, "#line$1");
}

// 拼 shader 的公共头部：GLSL 版本、片元输出变量，以及 Shadertoy 那套标准 uniform。
// Sound pass 的输出是 vec2（立体声），并多两个音频 uniform，其余与普通 pass 完全一致，
// 放一处免得改动 GLSL 版本或加 uniform 时要同时改好几个地方
static std::string glslHeader(bool isSound) {
    std::ostringstream shader;
    // ESSL 300 而不是桌面 GLSL：整段 shader 随后要交给 ANGLE 翻译，它只认 ES 版本。
    // 翻译的产物才是喂给驱动的桌面 GLSL
    shader << "#version 300 es\n\n";
    // ES 的片元着色器没有默认精度，float 不声明就编译不过
    shader << "precision highp float;\n";
    shader << "precision highp int;\n\n";

    shader << (isSound ? "out vec2 fragColor;\n\n" : "out vec4 fragColor;\n\n");
    shader << "uniform vec3 iResolution;\n";
    shader << "uniform float iTime;\n";
    shader << "uniform float iTimeDelta;\n";
    shader << "uniform int iFrame;\n";
    shader << "uniform float iFrameRate;\n";
    shader << "uniform vec4 iMouse;\n";
    shader << "uniform vec4 iDate;\n";
    if (isSound) {
        shader << "uniform int iSampleRate;\n";
        shader << "uniform int iSampleOffset;\n";
    }

    shader << "\n";
    for (int i = 0; i < 4; ++i) {
        shader << "uniform sampler2D iChannel" << i << ";\n";
    }
    shader << "uniform vec3 iChannelResolution[4];\n";
    shader << "uniform float iChannelTime[4];\n";

    shader << "\n";
    return shader.str();
}

RenderCore::RenderCore(const ShaderConfig& config, const RunOptions& options)
    : m_config(config), m_options(options) {}

void RenderCore::init(int width, int height) {
    m_width = width;
    m_height = height;

    initQuad();

    // 所有 pass 的结果都先画进这个输出目标，之后要么截图、要么贴到窗口。
    // 这样截图不依赖窗口的 back buffer（窗口最小化时它是 0×0）。
    m_outputTarget = std::make_unique<GLFramebuffer>();
    if (!m_outputTarget->create(m_width, m_height, GL_RGBA8)) {
        throw std::runtime_error("Failed to create output render target");
    }

    initKeyboardTexture();
    loadCommonCode();
    initPasses();
}

void RenderCore::initQuad() {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);
}

bool RenderCore::loadCommonCode() {
    const std::string& commonPath = m_config.getCommonPath();
    if (commonPath.empty()) {
        m_commonCode = "";
        return true;
    }

    std::filesystem::path fullPath = m_config.getBasePath() / commonPath;
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        std::cerr << "Cannot open common file: " << fullPath << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    m_commonCode = buffer.str();
    std::cout << "Loaded common code: " << fullPath << std::endl;
    return true;
}

void RenderCore::initPasses() {
    for (const auto& passConfig : m_config.getPasses()) {
        auto pass = std::make_unique<RenderPass>();
        pass->name = passConfig.name;
        pass->channels = passConfig.channels;
        pass->useWindowResolution = (passConfig.width == 0 || passConfig.height == 0);
        pass->width = passConfig.width > 0 ? passConfig.width : m_width;
        pass->height = passConfig.height > 0 ? passConfig.height : m_height;
        pass->isImage = passConfig.isImage();
        pass->isSound = passConfig.isSound();

        // 检查是否需要双缓冲（自引用）
        for (const auto& channel : pass->channels) {
            if (channel && channel->type == ChannelInput::Type::Buffer && channel->source == pass->name) {
                pass->useDoubleBuffer = true;
                break;
            }
        }

        // Sound 通道特殊处理
        if (pass->isSound) {
            // 图片导出没地方放音频，除非用户另外要 WAV；视频模式的音轨就靠它
            if (m_options.mode == RunMode::Images && m_options.audioDumpPath.empty()) {
                continue;
            }
            // 编译不过就当没这个 pass。留着的话每帧都会去跑一个没编译的 shader，
            // 读回采样的缓冲也还是空的，往空指针里写就是段错误
            if (!initSoundPass(*pass)) {
                continue;
            }
            m_soundPass = pass.get();
            m_passMap[pass->name] = pass.get();
            m_passes.push_back(std::move(pass));
            continue;
        }

        // 只有 Buffer 通道需要 FBO
        if (!pass->isImage) {
            pass->framebuffer = std::make_unique<GLFramebuffer>();
            if (!pass->framebuffer->create(pass->width, pass->height)) {
                std::cerr << "Failed to create FBO for " << pass->name << std::endl;
                continue;
            }

            if (pass->useDoubleBuffer) {
                pass->framebufferAlt = std::make_unique<GLFramebuffer>();
                if (!pass->framebufferAlt->create(pass->width, pass->height)) {
                    std::cerr << "Failed to create alternate FBO for " << pass->name << std::endl;
                    continue;
                }
            }
        }

        // 加载 shader
        if (!loadShader(*pass, passConfig)) {
            continue;
        }

        std::cout << "Initialized pass: " << pass->name
                  << " (" << pass->width << "x" << pass->height << ")"
                  << (pass->useDoubleBuffer ? " [double buffer]" : "")
                  << (pass->isImage ? " [image]" : "") << std::endl;

        m_passMap[pass->name] = pass.get();
        m_passes.push_back(std::move(pass));
    }

    // 没有 Image 通道就什么都不显示，导出也是一片黑。而名字拼错（比如写成小写 image）
    // 会被当成 Buffer 悄悄收下，这里点一句，省得对着黑屏排查
    bool hasImage = false;
    for (const auto& pass : m_passes) {
        if (pass->isImage) {
            hasImage = true;
            break;
        }
    }
    if (!hasImage) {
        std::cerr << "Warning: no pass named \"Image\" was initialized, so nothing will be rendered "
                     "or exported (pass names are case-sensitive)" << std::endl;
    }

    computeMustRunPasses();
}

bool RenderCore::loadShader(RenderPass& pass, const PassConfig& config) {
    std::filesystem::path fullPath = m_config.getBasePath() / config.shaderPath;

    // 读取 shader 文件
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        std::cerr << "Cannot open shader file: " << fullPath << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string shaderCode = buffer.str();

    // 合并 common 代码和 shader 代码
    std::string combinedCode;
    if (!m_commonCode.empty()) {
        combinedCode = m_commonCode + "\n\n" + shaderCode;
    } else {
        combinedCode = shaderCode;
    }

    GlslPreprocessor preprocessor;

    // 统一预处理合并后的代码
    std::string processedCode = preprocessor.process(combinedCode, m_config.getBasePath());
    if (processedCode.empty()) {
        std::cerr << "Shader preprocessing failed for " << fullPath << std::endl;
        return false;
    }

    // 包装预处理后的代码
    std::string fullShader = wrapProcessedShader(processedCode, pass.isImage);

    // 交给 ANGLE 翻成桌面 GLSL：ES 和桌面的语法差异由它抹平，未初始化的变量也在
    // 这一步补上零值（桌面驱动不保证那个，Shadertoy 的 shader 却依赖它）
    TranslatedShader translated = translateShader(fullShader);
    if (!translated.ok) {
        std::cerr << "Shader translation failed for " << pass.name << " (" << fullPath
                  << "):\n" << translated.log << std::endl;
        return false;
    }
    pass.nameMap = translated.uniforms;

    if (!pass.shader.loadFromMemory(translated.code, sf::Shader::Type::Fragment)) {
        std::cerr << "Shader compilation failed for " << pass.name << std::endl;
        return false;
    }

    cacheUniformLocations(pass);

    std::cout << "Loaded shader: " << fullPath << std::endl;
    return true;
}

void RenderCore::cacheUniformLocations(RenderPass& pass) {
    const GLuint program = pass.shader.getNativeHandle();
    // 查 location 用的得是 ANGLE 翻译后驱动里的名字，不是源码里写的那个
    auto loc = [&pass, program](const std::string& name) {
        const auto it = pass.nameMap.find(name);
        const std::string& actual = it == pass.nameMap.end() ? name : it->second;
        return glGetUniformLocation(program, actual.c_str());
    };

    pass.locIResolution = loc("iResolution");
    pass.locITime = loc("iTime");
    pass.locITimeDelta = loc("iTimeDelta");
    pass.locIFrame = loc("iFrame");
    pass.locIFrameRate = loc("iFrameRate");
    pass.locIMouse = loc("iMouse");
    pass.locIDate = loc("iDate");
    pass.locChannelResolution = loc("iChannelResolution");
    pass.locChannelTime = loc("iChannelTime");
    pass.locISampleRate = loc("iSampleRate");
    pass.locISampleOffset = loc("iSampleOffset");
    for (int i = 0; i < 4; ++i) {
        pass.locChannels[i] = loc("iChannel" + std::to_string(i));
    }
}

std::string RenderCore::wrapProcessedShader(const std::string& processedCode, bool isImage) {
    std::ostringstream shader;

    shader << glslHeader(false);

    // 让后面这段的行号从 1 数起。header 那二十几行是程序加的，报错时把它们
    // 算进去，行号就和 shader 源文件对不上了
    shader << "#line 1\n";

    // 添加预处理后的代码
    shader << stripLineFilenames(processedCode) << "\n";

    // 添加 main 函数。初值只是个哨兵：GLSL 的 out 参数是「被调用者定义」的，调用
    // 前给的值不作数，shader 只写 .rgb 时 .a 是未定义值，所以哨兵之后还得再盖一层。
    // 盖法按通道分，对应 Shadertoy 的 mImagePassFooter 和 MakeHeader_Buffer 两套
    // footer：Image 通道输出到屏幕，alpha 没有意义（还让窗口和导出的 PNG 带透明），
    // 钉死成 1；Buffer 通道的 alpha 是数据，流体、粒子这类 shader 拿它存帧间状态，
    // 必须原样透传
    shader << "void main() {\n";
    shader << "    vec4 color = vec4(1e20);\n";
    shader << "    float _frame = float(iFrame);\n";
    shader << "    mainImage(color, gl_FragCoord.xy);\n";
    if (isImage) {
        shader << "    fragColor = vec4(color.xyz, 1.0);\n";
    } else {
        shader << "    fragColor = color;\n";
    }
    shader << "}\n";

    return shader.str();
}

void RenderCore::resize(int width, int height) {
    m_width = width;
    m_height = height;

    if (m_outputTarget) {
        if (!m_outputTarget->create(m_width, m_height, GL_RGBA8)) {
            std::cerr << "Failed to resize output target" << std::endl;
        }
    }

    for (auto& pass : m_passes) {
        // 只调整使用窗口分辨率的 Buffer pass
        if (!pass->isImage && pass->useWindowResolution) {
            pass->width = m_width;
            pass->height = m_height;

            // 重新创建 FBO
            pass->framebuffer = std::make_unique<GLFramebuffer>();
            if (!pass->framebuffer->create(pass->width, pass->height)) {
                std::cerr << "Failed to resize FBO for " << pass->name << std::endl;
            }

            if (pass->useDoubleBuffer) {
                pass->framebufferAlt = std::make_unique<GLFramebuffer>();
                if (!pass->framebufferAlt->create(pass->width, pass->height)) {
                    std::cerr << "Failed to resize alternate FBO for " << pass->name << std::endl;
                }
            }

            std::cout << "Resized " << pass->name << " to " << pass->width << "x" << pass->height << std::endl;
        }
    }
}

void RenderCore::initKeyboardTexture() {
    m_keyboardTexture = std::make_unique<GLTexture>();

    glGenTextures(1, &m_keyboardTexture->id);
    glBindTexture(GL_TEXTURE_2D, m_keyboardTexture->id);

    m_keyboardTexture->width = 256;
    m_keyboardTexture->height = 3;

    // 初始化为全零
    std::vector<float> zeroData(256 * 3 * 4, 0.0f);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, 3, 0, GL_RGBA, GL_FLOAT, zeroData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_keyPressedPrev.fill(false);
}

void RenderCore::updateKeyboard(const std::array<bool, 256>& keys) {
    std::vector<float> data(256 * 3 * 4, 0.0f);

    for (int i = 0; i < 256; ++i) {
        // Row 0: current pressed state (keydown)
        if (keys[i]) {
            data[i * 4] = 1.0f;
        }
        // Row 1: just pressed (keypressed)
        if (keys[i] && !m_keyPressedPrev[i]) {
            data[(256 + i) * 4] = 1.0f;
        }
        // Row 2: toggle state
        static std::array<bool, 256> keyToggle{};
        if (keys[i] && !m_keyPressedPrev[i]) {
            keyToggle[i] = !keyToggle[i];
        }
        if (keyToggle[i]) {
            data[(512 + i) * 4] = 1.0f;
        }
    }

    glBindTexture(GL_TEXTURE_2D, m_keyboardTexture->id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 3, GL_RGBA, GL_FLOAT, data.data());

    m_keyPressedPrev = keys;
}

void RenderCore::renderBufferPasses(const FrameState& frame, bool intermediate) {
    // 渲染所有 Buffer pass（非 Image 非 Sound）
    for (auto& pass : m_passes) {
        if (pass->isImage || pass->isSound) continue;
        // 跳帧模式的中间帧只走状态链条上的通道。无状态的这一帧算出来也没人看：
        // 下一个目标帧会把它整个重算一遍
        if (intermediate && m_mustRunEveryFrame.find(pass.get()) == m_mustRunEveryFrame.end()) {
            continue;
        }
        renderPass(*pass, frame);
    }
}

// 跳帧模式的中间帧必须渲染哪些通道：输出传递依赖自己的（自引用、引用环）一帧不落地跑，
// 它们读到的通道也得跟着跑——补历史时被读的一方不在同一帧渲染，读到的就是上一次留下的
// 陈旧值。剩下的中间帧可以整个跳过，那是跳帧唯一能省下来的部分。
//
// 判据是"沿引用边能不能走回自己"，环也算：环里第一个渲染的读上一帧，整条环的值都是
// 跨帧演化出来的，跳了就对不上
void RenderCore::computeMustRunPasses() {
    // 每个 pass 读了哪些 buffer pass
    std::map<RenderPass*, std::vector<RenderPass*>> reads;
    auto addReader = [&](RenderPass* reader) {
        std::vector<RenderPass*>& deps = reads[reader];
        for (auto& channel : reader->channels) {
            if (!channel || channel->type != ChannelInput::Type::Buffer) continue;
            auto it = m_passMap.find(channel->source);
            if (it == m_passMap.end() || it->second->isImage) continue;
            deps.push_back(it->second);
        }
    };
    // Image 不用管：它只在目标帧渲染，那时所有 buffer 都是新鲜的
    for (auto& pass : m_passes) {
        if (!pass->isImage) addReader(pass.get());
    }

    auto reachesSelf = [&reads](RenderPass* start) {
        std::vector<RenderPass*> stack{start};
        std::set<RenderPass*> seen{start};
        while (!stack.empty()) {
            RenderPass* node = stack.back();
            stack.pop_back();
            auto it = reads.find(node);
            if (it == reads.end()) continue;
            for (RenderPass* next : it->second) {
                if (next == start) return true;
                if (seen.insert(next).second) stack.push_back(next);
            }
        }
        return false;
    };

    m_mustRunEveryFrame.clear();
    std::vector<RenderPass*> pending;
    for (auto& pass : m_passes) {
        if (pass->isImage || pass->isSound) continue;
        if (reachesSelf(pass.get())) {
            m_mustRunEveryFrame.insert(pass.get());
            pending.push_back(pass.get());
        }
    }

    // Sound 每帧都要生成采样，它读的 buffer 也得每帧是新鲜的。它自己不占 buffer 的
    // 名额（不归 renderBufferPasses 管），只当传播的起点
    if (m_soundPass) {
        for (RenderPass* dep : reads[m_soundPass]) {
            if (m_mustRunEveryFrame.insert(dep).second) pending.push_back(dep);
        }
    }

    // 沿着"读了谁"往外传：链条上的人补历史时，被读的一方不在同一帧渲染就会读岔
    while (!pending.empty()) {
        RenderPass* node = pending.back();
        pending.pop_back();
        for (RenderPass* dep : reads[node]) {
            if (m_mustRunEveryFrame.insert(dep).second) pending.push_back(dep);
        }
    }
}

GLTexture* RenderCore::getChannelTexture(const ChannelInput& input) {
    if (input.type == ChannelInput::Type::Keyboard) {
        return m_keyboardTexture.get();
    } else if (input.type == ChannelInput::Type::Buffer) {
        auto it = m_passMap.find(input.source);
        if (it != m_passMap.end()) {
            RenderPass* sourcePass = it->second;
            GLFramebuffer* fb = sourcePass->getReadTarget();
            if (fb) {
                return &fb->colorTex;
            }
        }
        std::cerr << "Buffer not found: " << input.source << std::endl;
        return nullptr;
    }
    return m_textures.get(input, m_config.getBasePath());
}

// 绑定 pass 的 4 个输入通道，并把各通道实际纹理的尺寸报到 iChannelResolution。
// 离屏的 buffer pass 和上屏的 Image pass 走的都是这一套，逻辑一模一样
void RenderCore::bindChannels(RenderPass& pass) {
    std::array<sf::Glsl::Vec3, 4> channelResolutions;

    for (int i = 0; i < 4; ++i) {
        channelResolutions[i] = sf::Glsl::Vec3(0.0f, 0.0f, 0.0f);
        if (!pass.channels[i]) continue;

        GLTexture* tex = getChannelTexture(*pass.channels[i]);
        if (!tex) continue;

        tex->bind(i);

        // 采样参数交给 sampler object，纹理自身状态一个字节都不动
        glBindSampler(i, m_textures.sampler(pass.channels[i]->filter, pass.channels[i]->wrap));

        // Buffer 内容每帧都变，mipmap 得重新生成；dirty 标记保证一帧内只生成一次
        if (pass.channels[i]->filter == ChannelInput::Filter::Mipmap
            && pass.channels[i]->type == ChannelInput::Type::Buffer
            && tex->mipmapDirty) {
            glGenerateMipmap(GL_TEXTURE_2D);
            tex->mipmapDirty = false;
        }

        glUniform1i(pass.locChannels[i], i);  // location 为 -1 时 glUniform 是 no-op
        channelResolutions[i] = sf::Glsl::Vec3(
            static_cast<float>(tex->width),
            static_cast<float>(tex->height), 1.0f);
    }
    if (pass.locChannelResolution >= 0) {
        glUniform3fv(pass.locChannelResolution, 4,
                     reinterpret_cast<const float*>(channelResolutions.data()));
    }
}

void RenderCore::updateUniforms(RenderPass& pass, int width, int height, const FrameState& frame) {
    float iFrameRate = (frame.timeDelta > 0) ? 1.0f / frame.timeDelta : 60.0f;

    sf::Shader& shader = pass.shader;
    sf::Shader::bind(&shader);

    // 全部走缓存的 location：按名字设等于每帧让 SFML 再查一遍 glGetUniformLocation
    if (pass.locIResolution >= 0) {
        glUniform3f(pass.locIResolution, static_cast<float>(width),
                    static_cast<float>(height), 1.0f);
    }
    if (pass.locITime >= 0) glUniform1f(pass.locITime, frame.time);
    if (pass.locITimeDelta >= 0) glUniform1f(pass.locITimeDelta, frame.timeDelta);
    if (pass.locIFrame >= 0) glUniform1i(pass.locIFrame, frame.frameIndex);
    if (pass.locIFrameRate >= 0) glUniform1f(pass.locIFrameRate, iFrameRate);

    if (pass.locIMouse >= 0) {
        glUniform4f(pass.locIMouse, frame.mouse.x, frame.mouse.y, frame.mouse.z,
                    frame.mouse.w);
    }

    if (pass.locIDate >= 0) {
        glUniform4f(pass.locIDate, frame.date[0], frame.date[1], frame.date[2], frame.date[3]);
    }

    if (pass.locChannelTime >= 0) {
        const std::array<float, 4> channelTimeValues = {frame.time, frame.time, frame.time,
                                                        frame.time};
        glUniform1fv(pass.locChannelTime, 4, channelTimeValues.data());
    }
}

void RenderCore::renderPass(RenderPass& pass, const FrameState& frame) {
    if (pass.isImage) return;

    GLFramebuffer* target = pass.getWriteTarget();

    // 绑定 FBO
    target->bind();
    glViewport(0, 0, pass.width, pass.height);
    // 这里刻意不清屏：Shadertoy 的 buffer 也不清，着色器里 discard 掉的像素因此会保留
    // 上一帧的内容，不少 shader 靠这个把数据攒在缓冲区里（比如 Voxel game Evolution
    // 只在头几帧生成材质纹理，之后全靠"不写就保留"）

    // 绑定 shader
    sf::Shader::bind(&pass.shader);

    // 设置 uniforms。宽高显式传：Image pass 的 width/height 不随窗口缩放更新，
    // renderToScreen 那边必须给窗口尺寸，不能图省事改用 pass 自己的
    updateUniforms(pass, pass.width, pass.height, frame);

    bindChannels(pass);

    // 渲染全屏四边形
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // 解绑 sampler，避免残留影响后续绘制（尤其 ImGui）
    for (int i = 0; i < 4; ++i) {
        glBindSampler(i, 0);
    }

    // 解绑
    GLFramebuffer::unbind();

    // 切换缓冲
    pass.swapBuffer();

    // 刚写入的内容变了，下次被采样成 mipmap 时要重新生成
    target->colorTex.mipmapDirty = true;
}

// 按名字找 pass，调试视图和 --dump-buffers 都靠它把命令行写的名字对上实际场景
RenderPass* RenderCore::findPass(const std::string& name) {
    auto it = m_passMap.find(name);
    return it == m_passMap.end() ? nullptr : it->second;
}

RenderPass* RenderCore::imagePass() {
    for (auto& pass : m_passes) {
        if (pass->isImage) return pass.get();
    }
    return nullptr;
}

RenderPass* RenderCore::debugViewPass(const std::string& wantName) {
    if (wantName.empty()) return nullptr;

    RenderPass* pass = findPass(wantName);
    // Sound 的输出是 vec2、也不上屏，没有能看的东西
    if (!pass || pass->isSound || !pass->framebuffer) return nullptr;
    return pass;
}

GLFramebuffer* RenderCore::currentViewTarget(const std::string& debugName) {
    RenderPass* debug = debugViewPass(debugName);
    return debug ? debug->getReadTarget() : m_outputTarget.get();
}

void RenderCore::blitToThumbnail(RenderPass& pass) {
    // Image pass 没有自己的 FBO（它直接画到输出目标），撞上就得退出，
    // 不能想当然以为凡是 pass 都有 framebuffer
    GLFramebuffer* src = pass.getReadTarget();
    if (!src) return;

    const int w = src->colorTex.width;
    const int h = src->colorTex.height;
    if (w <= 0 || h <= 0) return;

    // 长边固定，短边按比例。窗口缩放会改 buffer 尺寸，对不上就重建
    constexpr float THUMBNAIL_MAX = 120.0f;
    const float scale = THUMBNAIL_MAX / static_cast<float>(std::max(w, h));
    const int tw = std::max(1, static_cast<int>(w * scale));
    const int th = std::max(1, static_cast<int>(h * scale));

    std::unique_ptr<GLFramebuffer>& thumbnail = m_thumbnails[pass.name];
    if (!thumbnail || thumbnail->colorTex.width != tw || thumbnail->colorTex.height != th) {
        thumbnail = std::make_unique<GLFramebuffer>();
        if (!thumbnail->create(tw, th, GL_RGBA8)) {
            thumbnail.reset();
            return;
        }
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, src->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, thumbnail->fbo);
    glDisable(GL_SCISSOR_TEST);  // blit 受 draw framebuffer 的 scissor 影响
    glBlitFramebuffer(0, 0, w, h, 0, 0, tw, th, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);  // 恢复默认，ImGui/SFML 需要 FBO 0
}

void RenderCore::renderToScreen(const FrameState& frame, const std::string& debugName) {
    // 渲染到输出目标而不是直接画到窗口：截图和显示共用同一份内容，
    // 也避免直接读窗口的 back buffer（窗口最小化时它是 0×0）
    m_outputTarget->bind();
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    RenderPass* debug = debugViewPass(debugName);
    if (debug) {
        // 调试视图：把那个 pass 的 buffer 直接贴过来。刻意不重跑它的 shader——
        // buffer 是有状态的，重跑等于把这一帧的内容又算了一遍，看到的就不是现场了。
        // 源是浮点、输出目标是 8bit，这个转换由硬件做
        GLFramebuffer* src = debug->getReadTarget();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, src->fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_outputTarget->fbo);
        glDisable(GL_SCISSOR_TEST);  // blit 受 draw framebuffer 的 scissor 影响，ImGui 可能留下状态
        glBlitFramebuffer(0, 0, src->colorTex.width, src->colorTex.height,
                          0, 0, m_width, m_height,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);  // 恢复默认，ImGui/SFML 需要 FBO 0
        sf::Shader::bind(nullptr);
        return;
    }

    if (RenderPass* image = imagePass()) {
        sf::Shader::bind(&image->shader);
        // 这里给的是窗口尺寸而不是 pass 自己的：Image pass 不参与 resize 的尺寸更新，
        // 窗口缩放后它的 width/height 是过期的
        updateUniforms(*image, m_width, m_height, frame);

        bindChannels(*image);

        // 渲染全屏四边形
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        // 解绑 sampler，避免残留影响 ImGui 渲染
        for (int i = 0; i < 4; ++i) {
            glBindSampler(i, 0);
        }
    }

    sf::Shader::bind(nullptr);
    // display() 由调用方统一调用，以支持 ImGui
}

// 传进来的已经是预处理好的整段代码（common 和 pass 一起过一次），这里只套头部和 main。
// 公共代码不能在这儿再展开一遍：那样它会出现在结果里两次，只要有函数定义就是重复定义
std::string RenderCore::wrapSoundShader(const std::string& processedCode) {
    std::ostringstream shader;

    shader << glslHeader(true);
    // 同 wrapProcessedShader：header 的行数不该算进 shader 源文件的行号里
    shader << "#line 1\n";
    shader << stripLineFilenames(processedCode) << "\n";

    shader << "void main() {\n";
    shader << "    int samp = iSampleOffset + int(floor(gl_FragCoord.x));\n";
    shader << "    float time = float(samp) / float(iSampleRate);\n";
    shader << "    fragColor = mainSound(samp, time);\n";
    shader << "}\n";

    return shader.str();
}

bool RenderCore::initSoundPass(RenderPass& pass) {
    // 一批样本渲染成 batchSamples x 1 的纹理，宽度不能超过 GL_MAX_TEXTURE_SIZE，
    // 否则 FBO 建不出来（WSL 的 d3d12 后端上限只有 16384，撑不下 22050）
    GLint maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (maxTextureSize > 0 && maxTextureSize < m_soundBatchSamples) {
        m_soundBatchSamples = maxTextureSize;
    }

    // 创建 FBO 用于渲染音频样本
    pass.framebuffer = std::make_unique<GLFramebuffer>();
    if (!pass.framebuffer->create(m_soundBatchSamples, 1)) {
        std::cerr << "Failed to create FBO for Sound" << std::endl;
        return false;
    }

    // 加载 shader
    std::filesystem::path fullPath;
    for (const auto& pc : m_config.getPasses()) {
        if (pc.name == pass.name) {
            fullPath = m_config.getBasePath() / pc.shaderPath;
            break;
        }
    }

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        std::cerr << "Cannot open sound shader file: " << fullPath << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string userCode = buffer.str();

    // 公共代码和 pass 代码合在一次预处理里跑：宏状态是连续的，分开跑的话
    // pass 代码看不到公共代码里定义的宏，而公共代码自己也会被展开两遍
    std::string combinedCode;
    if (!m_commonCode.empty()) {
        combinedCode = m_commonCode + "\n\n" + userCode;
    } else {
        combinedCode = userCode;
    }

    GlslPreprocessor preprocessor;
    std::string processedCode = preprocessor.process(combinedCode, m_config.getBasePath());
    std::string fullShader = wrapSoundShader(processedCode);

    // 和普通 pass 一样过 ANGLE：Sound pass 也可能有没写初值的变量
    TranslatedShader translated = translateShader(fullShader);
    if (!translated.ok) {
        std::cerr << "Sound shader translation failed (" << fullPath << "):\n"
                  << translated.log << std::endl;
        return false;
    }
    pass.nameMap = translated.uniforms;

    if (!pass.shader.loadFromMemory(translated.code, sf::Shader::Type::Fragment)) {
        std::cerr << "Sound shader compilation failed" << std::endl;
        return false;
    }

    cacheUniformLocations(pass);

    m_soundFloatData.resize(static_cast<size_t>(m_soundBatchSamples) * 2);

    std::cout << "Initialized sound pass: " << m_soundBatchSamples << " samples per batch ("
              << (m_soundBatchSamples * 1000.0 / SOUND_SAMPLE_RATE) << "ms @ " << SOUND_SAMPLE_RATE << "Hz)" << std::endl;
    return true;
}

void RenderCore::renderSoundBatch(int batchSamples, const SoundState& state,
                                  std::vector<int16_t>& out) {
    if (batchSamples <= 0 || !m_soundPass) return;
    // 中转缓冲是 Sound pass 初始化时开好的。glGetTexImage 会照单全收地往 data() 里写，
    // 缓冲没开起来（初始化失败过）而这里没挡住的话就是段错误
    if (m_soundFloatData.size() < static_cast<size_t>(m_soundBatchSamples) * 2) return;

    // glGetTexImage 读的是整张纹理，缓冲得按 FBO 宽度来，不能按实际渲染的窄条
    std::vector<float>& floatData = m_soundFloatData;
    out.assign(static_cast<size_t>(batchSamples) * 2, 0);  // 立体声

    // 绑定 FBO
    m_soundPass->framebuffer->bind();
    glViewport(0, 0, batchSamples, 1);
    m_soundPass->framebuffer->clear();

    // 绑定 shader
    sf::Shader::bind(&m_soundPass->shader);

    // 设置 uniforms，只设 shader 里真的存在的（否则 SFML 每批都刷 not found 警告）
    RenderPass& sound = *m_soundPass;
    if (sound.locIResolution >= 0) {
        glUniform3f(sound.locIResolution, static_cast<float>(batchSamples), 1.0f, 1.0f);
    }
    if (sound.locITime >= 0) glUniform1f(sound.locITime, state.time);
    if (sound.locITimeDelta >= 0) glUniform1f(sound.locITimeDelta, 1.0f / SOUND_SAMPLE_RATE);
    if (sound.locIFrame >= 0) glUniform1i(sound.locIFrame, state.frame);
    if (sound.locIFrameRate >= 0) glUniform1f(sound.locIFrameRate, static_cast<float>(SOUND_SAMPLE_RATE));
    if (sound.locIMouse >= 0) {
        glUniform4f(sound.locIMouse, state.mouse.x, state.mouse.y, state.mouse.z,
                    state.mouse.w);
    }
    if (sound.locIDate >= 0) glUniform4f(sound.locIDate, 2024.0f, 1.0f, 1.0f, 0.0f);
    if (sound.locISampleRate >= 0) glUniform1i(sound.locISampleRate, SOUND_SAMPLE_RATE);
    if (sound.locISampleOffset >= 0) glUniform1i(sound.locISampleOffset, static_cast<int>(state.sampleOffset));

    // 设置 iChannel uniforms（绑定其他 Buffer 的纹理）
    for (int ch = 0; ch < 4; ++ch) {
        if (m_soundPass->channels[ch]) {
            GLTexture* tex = getChannelTexture(*m_soundPass->channels[ch]);
            if (tex) {
                glActiveTexture(GL_TEXTURE0 + ch);
                glBindTexture(GL_TEXTURE_2D, tex->id);
                glBindSampler(ch, m_textures.sampler(m_soundPass->channels[ch]->filter,
                                                     m_soundPass->channels[ch]->wrap));
                if (m_soundPass->locChannels[ch] >= 0) {
                    glUniform1i(m_soundPass->locChannels[ch], ch);  // 采样器取纹理单元 ch
                }
            }
        }
    }

    // iChannelTime[4]（与 updateUniforms 保持一致，否则 sound shader 读到 0）
    std::array<float, 4> channelTimeValues;
    for (int i = 0; i < 4; ++i) {
        channelTimeValues[i] = state.time;
    }
    if (m_soundPass->locChannelTime >= 0) {
        glUniform1fv(m_soundPass->locChannelTime, 4, channelTimeValues.data());
    }

    // 渲染
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    for (int i = 0; i < 4; ++i) {
        glBindSampler(i, 0);
    }

    GLFramebuffer::unbind();

    // 读取 FBO 数据
    glBindTexture(GL_TEXTURE_2D, m_soundPass->framebuffer->colorTex.id);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, floatData.data());

    // 转换为 16 位音频
    for (int i = 0; i < batchSamples; ++i) {
        float left = std::clamp(floatData[i * 2], -1.0f, 1.0f);
        float right = std::clamp(floatData[i * 2 + 1], -1.0f, 1.0f);
        out[i * 2] = static_cast<int16_t>(left * 32767);
        out[i * 2 + 1] = static_cast<int16_t>(right * 32767);
    }
}
