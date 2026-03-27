#include "shadertoy_emulator.hpp"
#include "glsl_preprocessor.hpp"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <ctime>

// 全屏四边形顶点数据
static const float QUAD_VERTICES[] = {
    // x, y
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f
};

ShadertoyEmulator::ShadertoyEmulator(const ShaderConfig& config, bool showFps)
    : m_width(config.getWidth()), m_height(config.getHeight()), m_showFps(showFps),
      m_config(config), m_frameCount(0) {

    // 创建窗口
    uint32_t windowStyle = m_config.isResizable()
        ? sf::Style::Default
        : (sf::Style::Titlebar | sf::Style::Close);
    m_window.create(sf::VideoMode({static_cast<unsigned int>(m_width),
                                    static_cast<unsigned int>(m_height)}),
                    m_config.getName().empty() ? "Shadertoy Emulator" : m_config.getName(),
                    windowStyle);
    m_window.setFramerateLimit(60);

    // 初始化 glad（必须在创建 OpenGL 上下文后）
    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return;
    }
    std::cout << "OpenGL " << GLVersion.major << "." << GLVersion.minor << std::endl;

    // 初始化 OpenGL 顶点数据
    initQuad();

    // 初始化键盘纹理
    initKeyboardTexture();

    // 加载 Common 代码
    loadCommonCode();

    // 初始化所有渲染通道
    initPasses();

    // 初始化时间
    m_startTime = std::chrono::high_resolution_clock::now();
    m_lastFrameTime = m_startTime;
}

void ShadertoyEmulator::initQuad() {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);
}

bool ShadertoyEmulator::loadCommonCode() {
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

void ShadertoyEmulator::initPasses() {
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
            m_soundPass = pass.get();  // 先保存指针，initSoundPass 里会用到
            initSoundPass(*pass);
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
}

bool ShadertoyEmulator::loadShader(RenderPass& pass, const PassConfig& config) {
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
    std::string fullShader = wrapProcessedShader(processedCode);

    if (!pass.shader.loadFromMemory(fullShader, sf::Shader::Type::Fragment)) {
        std::cerr << "Shader compilation failed for " << pass.name << std::endl;
        return false;
    }

    std::cout << "Loaded shader: " << fullPath << std::endl;
    return true;
}

std::string ShadertoyEmulator::wrapProcessedShader(const std::string& processedCode) {
    std::ostringstream shader;

    shader << "#version 330 core\n\n";
    shader << "out vec4 fragColor;\n\n";
    shader << "uniform vec3 iResolution;\n";
    shader << "uniform float iTime;\n";
    shader << "uniform float iTimeDelta;\n";
    shader << "uniform int iFrame;\n";
    shader << "uniform float iFrameRate;\n";
    shader << "uniform vec4 iMouse;\n";
    shader << "uniform vec4 iDate;\n";

    for (int i = 0; i < 4; ++i) {
        shader << "uniform sampler2D iChannel" << i << ";\n";
    }
    shader << "uniform vec3 iChannelResolution[4];\n";

    shader << "\n";

    // 添加预处理后的代码
    shader << processedCode << "\n";

    // 添加 main 函数
    shader << "void main() {\n";
    shader << "    float _frame = float(iFrame);\n";
    shader << "    mainImage(fragColor, gl_FragCoord.xy);\n";
    shader << "}\n";

    return shader.str();
}

std::string ShadertoyEmulator::wrapShader(const std::string& userCode) {
    GlslPreprocessor preprocessor;

    std::ostringstream shader;

    shader << "#version 330 core\n\n";
    shader << "out vec4 fragColor;\n\n";
    shader << "uniform vec3 iResolution;\n";
    shader << "uniform float iTime;\n";
    shader << "uniform float iTimeDelta;\n";
    shader << "uniform int iFrame;\n";
    shader << "uniform float iFrameRate;\n";
    shader << "uniform vec4 iMouse;\n";
    shader << "uniform vec4 iDate;\n";

    for (int i = 0; i < 4; ++i) {
        shader << "uniform sampler2D iChannel" << i << ";\n";
    }
    shader << "uniform vec3 iChannelResolution[4];\n";

    shader << "\n";

    if (!m_commonCode.empty()) {
        shader << "// === Common Code ===\n";
        shader << preprocessor.process(m_commonCode, m_config.getBasePath()) << "\n";
    }

    shader << "// === Pass Code ===\n";
    shader << preprocessor.continueProcess(userCode, m_config.getBasePath()) << "\n";

    shader << "void main() {\n";
    shader << "    float _frame = float(iFrame);\n";
    shader << "    mainImage(fragColor, gl_FragCoord.xy);\n";
    shader << "}\n";

    return shader.str();
}

void ShadertoyEmulator::run() {
    sf::Clock clock;

    while (m_window.isOpen()) {
        handleEvents();
        renderPasses();
        renderToScreen();

        if (m_showFps) {
            float fps = 1.0f / clock.restart().asSeconds();
            std::cout << "\rFPS: " << std::fixed << std::setprecision(1) << fps << "   " << std::flush;
        }
    }
}

void ShadertoyEmulator::handleEvents() {
    while (auto event = m_window.pollEvent()) {
        if (event.has_value()) {
            if (event->is<sf::Event::Closed>()) {
                m_window.close();
            }
            else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
                // 更新按下时的位置（如果正在按下）
                if (m_mouseDown) {
                    m_mouseDownX = static_cast<float>(mouseMoved->position.x);
                    m_mouseDownY = static_cast<float>(m_height - mouseMoved->position.y);
                }
            }
            else if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                m_mouseDown = true;
                m_mouseJustClicked = true;
                float x = static_cast<float>(mousePressed->position.x);
                float y = static_cast<float>(m_height - mousePressed->position.y);
                m_mouseDownX = x;
                m_mouseDownY = y;
                m_mouseClickX = x;
                m_mouseClickY = y;
            }
            else if (event->is<sf::Event::MouseButtonReleased>()) {
                m_mouseDown = false;
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                m_width = static_cast<int>(resized->size.x);
                m_height = static_cast<int>(resized->size.y);
                m_window.setView(sf::View(sf::FloatRect({0.0f, 0.0f},
                                          sf::Vector2f(static_cast<float>(m_width),
                                                      static_cast<float>(m_height)))));
                resizeFramebuffers();
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                int keyCode = mapSfmlKeyToShadertoy(keyPressed->code);
                if (keyCode < 0) {
                    keyCode = mapSfmlScancodeToShadertoy(keyPressed->scancode);
                }
                if (keyCode >= 0 && keyCode < 256) {
                    m_keyPressed[keyCode] = true;
                }
                if (keyPressed->code == sf::Keyboard::Key::Escape) {
                    m_window.close();
                }
            }
            else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
                int keyCode = mapSfmlKeyToShadertoy(keyReleased->code);
                if (keyCode < 0) {
                    keyCode = mapSfmlScancodeToShadertoy(keyReleased->scancode);
                }
                if (keyCode >= 0 && keyCode < 256) {
                    m_keyPressed[keyCode] = false;
                }
            }
        }
    }
}

void ShadertoyEmulator::updateUniforms(sf::Shader& shader, int width, int height) {
    auto now = std::chrono::high_resolution_clock::now();

    float iTime = std::chrono::duration<float>(now - m_startTime).count();
    float iTimeDelta = std::chrono::duration<float>(now - m_lastFrameTime).count();
    float iFrameRate = (iTimeDelta > 0) ? 1.0f / iTimeDelta : 60.0f;

    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    float iDateYear = static_cast<float>(tm->tm_year + 1900);
    float iDateMonth = static_cast<float>(tm->tm_mon + 1);
    float iDateDay = static_cast<float>(tm->tm_mday);
    float iDateSec = static_cast<float>(tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);

    sf::Shader::bind(&shader);
    shader.setUniform("iResolution", sf::Glsl::Vec3(static_cast<float>(width),
                                                      static_cast<float>(height), 1.0f));
    shader.setUniform("iTime", iTime);
    shader.setUniform("iTimeDelta", iTimeDelta);
    shader.setUniform("iFrame", m_frameCount);
    shader.setUniform("iFrameRate", iFrameRate);

    // iMouse:
    // xy = 按下时的位置
    // z = 按下 ? 点击位置 : -点击位置
    // w = 刚点击 ? 点击位置 : -点击位置
    float z = m_mouseDown ? m_mouseClickX : -m_mouseClickX;
    float w = m_mouseJustClicked ? m_mouseClickY : -m_mouseClickY;
    shader.setUniform("iMouse", sf::Glsl::Vec4(m_mouseDownX, m_mouseDownY, z, w));

    shader.setUniform("iDate", sf::Glsl::Vec4(iDateYear, iDateMonth, iDateDay, iDateSec));
}

void ShadertoyEmulator::resizeFramebuffers() {
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

void ShadertoyEmulator::renderPasses() {
    // 更新键盘纹理
    updateKeyboardTexture();

    // 1. 先渲染所有 Buffer pass（非 Image 非 Sound）
    for (auto& pass : m_passes) {
        if (!pass->isImage && !pass->isSound) {
            renderPass(*pass);
        }
    }

    // 2. 检查并生成声音（此时可以读取当前帧的 Buffer 数据）
    checkAndGenerateSound();

    // 3. 最后渲染 Image pass
    for (auto& pass : m_passes) {
        if (pass->isImage) {
            renderPass(*pass);
        }
    }

    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    m_frameCount++;

    // 清除刚点击标记（只持续一帧）
    m_mouseJustClicked = false;
}

const GLTexture* ShadertoyEmulator::getChannelTexture(const ChannelInput& input) {
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
    } else {
        // 加载文件纹理 - 缓存 key 包含所有配置
        std::string cacheKey = input.source
            + (input.flipY ? ":flip" : "")
            + ":" + std::to_string(static_cast<int>(input.filter))
            + ":" + std::to_string(static_cast<int>(input.wrap));
        auto it = m_textureCache.find(cacheKey);
        if (it != m_textureCache.end()) {
            return it->second.get();
        }

        // 从文件加载
        if (loadTextureFile(input)) {
            return m_textureCache[cacheKey].get();
        }
        return nullptr;
    }
}

bool ShadertoyEmulator::loadTextureFile(const ChannelInput& input) {
    std::filesystem::path fullPath = m_config.getBasePath() / input.source;

    // 使用 SFML 加载图片
    sf::Image image;
    if (!image.loadFromFile(fullPath.string())) {
        std::cerr << "Failed to load texture: " << fullPath << std::endl;
        return false;
    }

    // 如果需要翻转，使用 SFML 的 flip 函数
    if (input.flipY) {
        image.flipVertically();
    }

    auto texture = std::make_unique<GLTexture>();

    glGenTextures(1, &texture->id);
    glBindTexture(GL_TEXTURE_2D, texture->id);

    sf::Vector2u size = image.getSize();
    texture->width = static_cast<int>(size.x);
    texture->height = static_cast<int>(size.y);

    // 使用 GL_RGBA32F 存储纹理数据
    std::vector<float> floatData(size.x * size.y * 4);
    const uint8_t* pixelData = image.getPixelsPtr();

    for (size_t i = 0; i < size.x * size.y * 4; i += 4) {
        floatData[i + 0] = pixelData[i + 0] / 255.0f;
        floatData[i + 1] = pixelData[i + 1] / 255.0f;
        floatData[i + 2] = pixelData[i + 2] / 255.0f;
        floatData[i + 3] = pixelData[i + 3] / 255.0f;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, size.x, size.y, 0, GL_RGBA, GL_FLOAT, floatData.data());

    // 设置 filter
    switch (input.filter) {
        case ChannelInput::Filter::Nearest:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case ChannelInput::Filter::Mipmap:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glGenerateMipmap(GL_TEXTURE_2D);
            break;
        case ChannelInput::Filter::Linear:
        default:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
    }

    // 设置 wrap
    switch (input.wrap) {
        case ChannelInput::Wrap::Repeat:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            break;
        case ChannelInput::Wrap::Mirror:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
            break;
        case ChannelInput::Wrap::Clamp:
        default:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            break;
    }

    // 缓存 key 包含所有配置
    std::string cacheKey = input.source
        + (input.flipY ? ":flip" : "")
        + ":" + std::to_string(static_cast<int>(input.filter))
        + ":" + std::to_string(static_cast<int>(input.wrap));
    m_textureCache[cacheKey] = std::move(texture);
    std::cout << "Loaded texture: " << fullPath << std::endl;
    return true;
}

void ShadertoyEmulator::renderPass(RenderPass& pass) {
    if (pass.isImage) return;

    GLFramebuffer* target = pass.getWriteTarget();

    // 绑定 FBO
    target->bind();
    glViewport(0, 0, pass.width, pass.height);
    target->clear();

    // 绑定 shader
    sf::Shader::bind(&pass.shader);

    // 设置 uniforms
    updateUniforms(pass.shader, pass.width, pass.height);

    // 手动绑定纹理通道
    GLuint shaderId = pass.shader.getNativeHandle();
    std::array<sf::Glsl::Vec3, 4> channelResolutions;

    for (int i = 0; i < 4; ++i) {
        if (pass.channels[i]) {
            const GLTexture* tex = getChannelTexture(*pass.channels[i]);
            if (tex) {
                tex->bind(i);

                // 设置 filter
                GLenum filter = (pass.channels[i]->filter == ChannelInput::Filter::Nearest)
                              ? GL_NEAREST : GL_LINEAR;
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

                // 设置 wrap
                GLenum wrap = GL_CLAMP_TO_EDGE;
                if (pass.channels[i]->wrap == ChannelInput::Wrap::Repeat) {
                    wrap = GL_REPEAT;
                } else if (pass.channels[i]->wrap == ChannelInput::Wrap::Mirror) {
                    wrap = GL_MIRRORED_REPEAT;
                }
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

                GLint loc = glGetUniformLocation(shaderId, ("iChannel" + std::to_string(i)).c_str());
                glUniform1i(loc, i);
                channelResolutions[i] = sf::Glsl::Vec3(
                    static_cast<float>(tex->width),
                    static_cast<float>(tex->height), 1.0f);
            }
        } else {
            channelResolutions[i] = sf::Glsl::Vec3(0.0f, 0.0f, 0.0f);
        }
    }
    pass.shader.setUniformArray("iChannelResolution", channelResolutions.data(), 4);

    // 渲染全屏四边形
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // 解绑
    GLFramebuffer::unbind();

    // 切换缓冲
    pass.swapBuffer();
}

void ShadertoyEmulator::renderToScreen() {
    // 绑定默认 FBO（窗口）
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 找到 Image 通道并渲染到屏幕
    for (auto& pass : m_passes) {
        if (pass->isImage) {
            sf::Shader::bind(&pass->shader);
            updateUniforms(pass->shader, m_width, m_height);

            GLuint shaderId = pass->shader.getNativeHandle();
            std::array<sf::Glsl::Vec3, 4> channelResolutions;

            for (int i = 0; i < 4; ++i) {
                if (pass->channels[i]) {
                    const GLTexture* tex = getChannelTexture(*pass->channels[i]);
                    if (tex) {
                        tex->bind(i);

                        // 设置 filter
                        GLenum filter = (pass->channels[i]->filter == ChannelInput::Filter::Nearest)
                                      ? GL_NEAREST : GL_LINEAR;
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

                        // 设置 wrap
                        GLenum wrap = GL_CLAMP_TO_EDGE;
                        if (pass->channels[i]->wrap == ChannelInput::Wrap::Repeat) {
                            wrap = GL_REPEAT;
                        } else if (pass->channels[i]->wrap == ChannelInput::Wrap::Mirror) {
                            wrap = GL_MIRRORED_REPEAT;
                        }
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

                        GLint loc = glGetUniformLocation(shaderId, ("iChannel" + std::to_string(i)).c_str());
                        glUniform1i(loc, i);
                        channelResolutions[i] = sf::Glsl::Vec3(
                            static_cast<float>(tex->width),
                            static_cast<float>(tex->height), 1.0f);
                    } else {
                        channelResolutions[i] = sf::Glsl::Vec3(0.0f, 0.0f, 0.0f);
                    }
                } else {
                    channelResolutions[i] = sf::Glsl::Vec3(0.0f, 0.0f, 0.0f);
                }
            }
            pass->shader.setUniformArray("iChannelResolution", channelResolutions.data(), 4);

            // 渲染全屏四边形
            glBindVertexArray(m_vao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);

            break;
        }
    }

    sf::Shader::bind(nullptr);
    m_window.display();
}

void ShadertoyEmulator::initKeyboardTexture() {
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

    // 初始化键盘状态数组
    m_keyPressed.fill(false);
    m_keyPressedPrev.fill(false);
}

void ShadertoyEmulator::updateKeyboardTexture() {
    std::vector<float> data(256 * 3 * 4, 0.0f);

    for (int i = 0; i < 256; ++i) {
        // Row 0: current pressed state (keydown)
        if (m_keyPressed[i]) {
            data[i * 4] = 1.0f;
        }
        // Row 1: just pressed (keypressed)
        if (m_keyPressed[i] && !m_keyPressedPrev[i]) {
            data[(256 + i) * 4] = 1.0f;
        }
        // Row 2: toggle state
        static std::array<bool, 256> keyToggle{};
        if (m_keyPressed[i] && !m_keyPressedPrev[i]) {
            keyToggle[i] = !keyToggle[i];
        }
        if (keyToggle[i]) {
            data[(512 + i) * 4] = 1.0f;
        }
    }

    glBindTexture(GL_TEXTURE_2D, m_keyboardTexture->id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 3, GL_RGBA, GL_FLOAT, data.data());

    m_keyPressedPrev = m_keyPressed;
}

int ShadertoyEmulator::mapSfmlKeyToShadertoy(sf::Keyboard::Key key) {
    // 映射到 JavaScript keyCode (0-255)
    // 参考: https://www.runoob.com/note/29592

    using K = sf::Keyboard::Key;

    // 字母键 A-Z: 65-90
    if (key >= K::A && key <= K::Z) {
        return static_cast<int>('A') + static_cast<int>(key) - static_cast<int>(K::A);
    }

    // 数字键 0-9: 48-57
    if (key >= K::Num0 && key <= K::Num9) {
        return static_cast<int>('0') + static_cast<int>(key) - static_cast<int>(K::Num0);
    }

    // 特殊键
    switch (key) {
        case K::Backspace:   return 8;
        case K::Tab:         return 9;
        case K::Enter:       return 13;
        case K::LShift:
        case K::RShift:      return 16;
        case K::LControl:
        case K::RControl:    return 17;
        case K::LAlt:
        case K::RAlt:        return 18;
        case K::Escape:      return 27;
        case K::Space:       return 32;
        case K::PageUp:      return 33;
        case K::PageDown:    return 34;
        case K::End:         return 35;
        case K::Home:        return 36;
        case K::Left:        return 37;
        case K::Up:          return 38;
        case K::Right:       return 39;
        case K::Down:        return 40;
        case K::Insert:      return 45;
        case K::Delete:      return 46;
        // F键: 112-123
        case K::F1:          return 112;
        case K::F2:          return 113;
        case K::F3:          return 114;
        case K::F4:          return 115;
        case K::F5:          return 116;
        case K::F6:          return 117;
        case K::F7:          return 118;
        case K::F8:          return 119;
        case K::F9:          return 120;
        case K::F10:         return 121;
        case K::F11:         return 122;
        case K::F12:         return 123;
        // 数字小键盘: 96-111
        case K::Numpad0:     return 96;
        case K::Numpad1:     return 97;
        case K::Numpad2:     return 98;
        case K::Numpad3:     return 99;
        case K::Numpad4:     return 100;
        case K::Numpad5:     return 101;
        case K::Numpad6:     return 102;
        case K::Numpad7:     return 103;
        case K::Numpad8:     return 104;
        case K::Numpad9:     return 105;
        case K::Multiply:    return 106;  // *
        case K::Add:         return 107;  // +
        case K::Subtract:    return 109;  // -
        case K::Divide:      return 111;  // /
        // 符号键
        case K::Semicolon:   return 186;  // ;:
        case K::Equal:       return 187;  // =+
        case K::Comma:       return 188;  // ,<
        case K::Hyphen:      return 189;  // -_
        case K::Period:      return 190;  // .>
        case K::Slash:       return 191;  // /?
        case K::Grave:       return 192;  // `~
        case K::LBracket:    return 219;  // [{
        case K::Backslash:   return 220;  // \|
        case K::RBracket:    return 221;  // ]}
        case K::Apostrophe:  return 222;  // '"
        default:             return -1;   // 未映射的键
    }
}

int ShadertoyEmulator::mapSfmlScancodeToShadertoy(sf::Keyboard::Scancode scancode) {
    // 映射 SFML Scancode 到 JavaScript keyCode
    // 用于处理 Key 枚举中没有的按键

    using S = sf::Keyboard::Scancode;

    switch (scancode) {
        case S::CapsLock:       return 20;
        case S::NumLock:        return 144;
        case S::ScrollLock:     return 145;
        case S::PrintScreen:    return 44;
        case S::Pause:          return 19;
        // 数字小键盘
        case S::NumpadDecimal:  return 110;  // .
        case S::NumpadDivide:   return 111;  // /
        case S::NumpadMultiply: return 106;  // *
        case S::NumpadMinus:    return 109;  // -
        case S::NumpadPlus:     return 107;  // +
        case S::NumpadEnter:    return 13;
        // 多媒体键
        case S::VolumeMute:     return 173;
        case S::VolumeDown:     return 174;
        case S::VolumeUp:       return 175;
        case S::MediaStop:      return 179;
        case S::MediaPlayPause: return 179;
        default:                return -1;
    }
}

// ========== 声音着色器实现 ==========

std::string ShadertoyEmulator::wrapSoundShader(const std::string& userCode) {
    GlslPreprocessor preprocessor;

    std::ostringstream shader;

    shader << "#version 330 core\n\n";
    shader << "out vec2 fragColor;\n\n";
    shader << "uniform vec3 iResolution;\n";
    shader << "uniform float iTime;\n";
    shader << "uniform float iTimeDelta;\n";
    shader << "uniform int iFrame;\n";
    shader << "uniform float iFrameRate;\n";
    shader << "uniform vec4 iMouse;\n";
    shader << "uniform vec4 iDate;\n";
    shader << "uniform int iSampleRate;\n";
    shader << "uniform int iSampleOffset;\n";

    for (int i = 0; i < 4; ++i) {
        shader << "uniform sampler2D iChannel" << i << ";\n";
    }
    shader << "uniform vec3 iChannelResolution[4];\n";

    shader << "\n";

    if (!m_commonCode.empty()) {
        shader << "// === Common Code ===\n";
        shader << preprocessor.process(m_commonCode, m_config.getBasePath()) << "\n";
    }

    shader << "// === Sound Pass Code ===\n";
    shader << preprocessor.continueProcess(userCode, m_config.getBasePath()) << "\n";

    shader << "void main() {\n";
    shader << "    int samp = iSampleOffset + int(floor(gl_FragCoord.x));\n";
    shader << "    float time = float(samp) / float(iSampleRate);\n";
    shader << "    fragColor = mainSound(samp, time);\n";
    shader << "}\n";

    return shader.str();
}

void ShadertoyEmulator::initSoundPass(RenderPass& pass) {
    // 创建 FBO 用于渲染音频样本
    pass.framebuffer = std::make_unique<GLFramebuffer>();
    if (!pass.framebuffer->create(SOUND_BATCH_SAMPLES, 1)) {
        std::cerr << "Failed to create FBO for Sound" << std::endl;
        return;
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
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string userCode = buffer.str();

    // 合并 common 代码
    std::string combinedCode;
    if (!m_commonCode.empty()) {
        combinedCode = m_commonCode + "\n\n" + userCode;
    } else {
        combinedCode = userCode;
    }

    GlslPreprocessor preprocessor;
    std::string processedCode = preprocessor.process(combinedCode, m_config.getBasePath());
    std::string fullShader = wrapSoundShader(processedCode);

    if (!pass.shader.loadFromMemory(fullShader, sf::Shader::Type::Fragment)) {
        std::cerr << "Sound shader compilation failed" << std::endl;
        return;
    }

    // 创建音频流
    m_soundStream = std::make_unique<SoundShaderStream>();
    m_soundStream->init(SOUND_SAMPLE_RATE);

    // 预生成 6 个缓冲区（3秒）
    for (int i = 0; i < 6; ++i) {
        generateSoundBatch();
    }

    m_soundStream->play();

    std::cout << "Initialized sound pass: " << SOUND_BATCH_SAMPLES << " samples per batch ("
              << (SOUND_BATCH_SAMPLES * 1000.0 / SOUND_SAMPLE_RATE) << "ms @ " << SOUND_SAMPLE_RATE << "Hz)" << std::endl;
}

void ShadertoyEmulator::checkAndGenerateSound() {
    if (!m_soundPass || !m_soundStream) return;

    // 保持至少 5 个就绪缓冲区
    if (m_soundStream->getReadyBufferCount() < 5) {
        generateSoundBatch();
    }
}

void ShadertoyEmulator::generateSoundBatch() {
    if (!m_soundPass) return;

    const int batchSamples = SOUND_BATCH_SAMPLES;
    std::vector<float> floatData(batchSamples * 2);
    std::vector<int16_t> audioSamples(batchSamples * 2);  // 立体声

    // 计算当前时间
    float iTime = static_cast<float>(m_soundSamplePosition) / SOUND_SAMPLE_RATE;

    // 绑定 FBO
    m_soundPass->framebuffer->bind();
    glViewport(0, 0, batchSamples, 1);
    m_soundPass->framebuffer->clear();

    // 绑定 shader
    sf::Shader::bind(&m_soundPass->shader);

    // 设置 uniforms
    m_soundPass->shader.setUniform("iResolution", sf::Glsl::Vec3(static_cast<float>(batchSamples), 1.0f, 1.0f));
    m_soundPass->shader.setUniform("iTime", iTime);
    m_soundPass->shader.setUniform("iTimeDelta", 1.0f / SOUND_SAMPLE_RATE);
    m_soundPass->shader.setUniform("iFrame", m_frameCount);
    m_soundPass->shader.setUniform("iFrameRate", static_cast<float>(SOUND_SAMPLE_RATE));
    m_soundPass->shader.setUniform("iMouse", sf::Glsl::Vec4(m_mouseDownX, m_mouseDownY, m_mouseClickX, m_mouseClickY));
    m_soundPass->shader.setUniform("iDate", sf::Glsl::Vec4(2024.0f, 1.0f, 1.0f, 0.0f));
    m_soundPass->shader.setUniform("iSampleRate", SOUND_SAMPLE_RATE);
    m_soundPass->shader.setUniform("iSampleOffset", static_cast<int>(m_soundSamplePosition));

    // 设置 iChannel uniforms（绑定其他 Buffer 的纹理）
    for (int ch = 0; ch < 4; ++ch) {
        std::string uniformName = "iChannel" + std::to_string(ch);
        if (m_soundPass->channels[ch]) {
            const GLTexture* tex = getChannelTexture(*m_soundPass->channels[ch]);
            if (tex) {
                glActiveTexture(GL_TEXTURE0 + ch);
                glBindTexture(GL_TEXTURE_2D, tex->id);
                m_soundPass->shader.setUniform(uniformName, sf::Shader::CurrentTexture);
            }
        }
    }

    // 渲染
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    GLFramebuffer::unbind();

    // 读取 FBO 数据
    glBindTexture(GL_TEXTURE_2D, m_soundPass->framebuffer->colorTex.id);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, floatData.data());

    // 转换为 16 位音频
    for (int i = 0; i < batchSamples; ++i) {
        float left = std::clamp(floatData[i * 2], -1.0f, 1.0f);
        float right = std::clamp(floatData[i * 2 + 1], -1.0f, 1.0f);
        audioSamples[i * 2] = static_cast<int16_t>(left * 32767);
        audioSamples[i * 2 + 1] = static_cast<int16_t>(right * 32767);
    }

    // 推送到音频流
    m_soundStream->pushSamples(audioSamples);
    m_soundSamplePosition += batchSamples;

    // 调试：保存前几批到WAV文件
    static int batchCount = 0;
    static std::vector<int16_t> allSamples;
    if (batchCount < 150) {
        allSamples.insert(allSamples.end(), audioSamples.begin(), audioSamples.end());
        batchCount++;
        if (batchCount == 150) {
            // 保存为WAV文件
            FILE* f = fopen("sound_debug.wav", "wb");
            if (f) {
                // WAV header
                int numSamples = allSamples.size() / 2;
                int dataSize = allSamples.size() * sizeof(int16_t);
                int fileSize = 36 + dataSize;

                fwrite("RIFF", 1, 4, f);
                fwrite(&fileSize, 4, 1, f);
                fwrite("WAVE", 1, 4, f);
                fwrite("fmt ", 1, 4, f);
                int fmtSize = 16;
                fwrite(&fmtSize, 4, 1, f);
                short audioFormat = 1; // PCM
                short channels = 2;
                int sampleRate = SOUND_SAMPLE_RATE;
                int byteRate = sampleRate * channels * 2;
                short blockAlign = channels * 2;
                short bitsPerSample = 16;
                fwrite(&audioFormat, 2, 1, f);
                fwrite(&channels, 2, 1, f);
                fwrite(&sampleRate, 4, 1, f);
                fwrite(&byteRate, 4, 1, f);
                fwrite(&blockAlign, 2, 1, f);
                fwrite(&bitsPerSample, 2, 1, f);
                fwrite("data", 1, 4, f);
                fwrite(&dataSize, 4, 1, f);
                fwrite(allSamples.data(), sizeof(int16_t), allSamples.size(), f);
                fclose(f);
                std::cout << "Saved sound_debug.wav (" << numSamples << " samples, "
                          << (float)numSamples / SOUND_SAMPLE_RATE << "s)" << std::endl;
            }
        }
    }
}
