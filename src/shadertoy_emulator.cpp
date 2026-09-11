#include "shadertoy_emulator.hpp"
#include "debug_overlay.hpp"
#include "audio_format.hpp"
#include <glad/glad.h>
#include <imgui-SFML.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ctime>
#include <cmath>
#include <stdexcept>
#include <algorithm>

// 建出导出要用的目录，失败时错误信息在这里就打了，调用方只管退出
static bool ensureDir(const std::filesystem::path& dir) {
    if (dir.empty()) return true;

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        std::cerr << "Cannot create output directory: " << dir << " (" << ec.message() << ")"
                  << std::endl;
        return false;
    }
    return true;
}

ShadertoyEmulator::ShadertoyEmulator(const ShaderConfig& config, const RunOptions& options)
    : m_options(options), m_width(config.getWidth()), m_height(config.getHeight()),
      m_config(config), m_core(config, options), m_exporter(options),
      m_enableGui(options.enableGui), m_debugViewPass(options.debugViewPass),
      m_frameCount(0) {

    // 离屏模式没有窗口，ImGui 没有可依附的目标
    if (m_options.isOffscreen()) {
        m_enableGui = false;
    }

    if (m_options.isOffscreen()) {
        // sf::Context 构造即创建并激活一个不依附窗口的 OpenGL 上下文
        m_context = std::make_unique<sf::Context>();
    } else {
        uint32_t windowStyle = m_config.isResizable()
            ? sf::Style::Default
            : (sf::Style::Titlebar | sf::Style::Close);
        m_window.create(sf::VideoMode({static_cast<unsigned int>(m_width),
                                        static_cast<unsigned int>(m_height)}),
                        m_config.getName().empty() ? "Shadertoy Emulator" : m_config.getName(),
                        windowStyle);
        m_window.setFramerateLimit(60);
    }

    // 初始化 glad（必须在创建 OpenGL 上下文后）
    if (!gladLoadGL()) {
        throw std::runtime_error("Failed to initialize GLAD");
    }
    // 带上 renderer/vendor：软渲染和硬件渲染的行为能差很远，出问题时这两行是第一条线索
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    std::cout << "OpenGL " << GLVersion.major << "." << GLVersion.minor << " ("
              << (renderer ? renderer : "?") << ", " << (vendor ? vendor : "?") << ")"
              << std::endl;

    // 输出目标、键盘纹理、common 代码和所有 pass 都由渲染核心自己建
    m_core.init(m_width, m_height);
    m_exporter.setOutputTarget(m_core.outputTarget(), m_width, m_height);

    // 音频流只为播放而建，导出模式只要数据。预生成 6 个缓冲区（3秒）。
    // 这里不 play()：音频的起步时刻要跟画面时间基准对齐，见 runWindow
    if (m_core.hasSoundPass() && !m_options.isOffscreen()) {
        m_soundStream = std::make_unique<SoundShaderStream>();
        m_soundStream->init(SOUND_SAMPLE_RATE);
        for (int i = 0; i < 6; ++i) {
            generateSoundBatch();
        }
    }

    // 初始化时间
    m_startTime = std::chrono::high_resolution_clock::now();
    m_lastFrameTime = m_startTime;

    // 调试面板。初始化失败就当没开 GUI，否则后面每帧的 Update/Render 都会崩
    if (m_enableGui) {
        m_overlay = std::make_unique<DebugOverlay>(*this);
        if (!m_overlay->init(m_window)) {
            m_enableGui = false;
        }
    }
}

ShadertoyEmulator::~ShadertoyEmulator() = default;

void ShadertoyEmulator::run() {
    switch (m_options.mode) {
        case RunMode::Window:
            runWindow();
            break;
        case RunMode::Images:
            runImages();
            break;
        case RunMode::Video:
            runVideo();
            break;
    }
}

void ShadertoyEmulator::runWindow() {
    // 画面的 iTime 和音频的采样位置是两套独立的时间轴，起点必须对齐：
    // 在 initPasses 里就 play() 的话，中间那堆初始化都变成音频超前画面的固定偏移
    m_startTime = std::chrono::high_resolution_clock::now();
    m_lastFrameTime = m_startTime;
    if (m_soundStream) {
        m_soundStream->play();
    }

    sf::Clock fpsClock;
    sf::Clock imguiDeltaClock;

    while (m_window.isOpen()) {
        // 计算当前 FPS
        float deltaTime = fpsClock.restart().asSeconds();
        m_currentFps = deltaTime > 0 ? 1.0f / deltaTime : 0.0f;

        handleEvents();

        // 音频重同步（恢复播放、重置）要重建缓冲，会绑定 FBO、改 viewport，
        // 不能塞在 ImGui 的渲染回调里做，统一挪到这儿
        if (m_audioResyncPending) {
            m_audioResyncPending = false;
            resyncAudio(m_audioResyncTarget);
        }

        // 暂停时只在有事件时渲染
        bool shouldRender = !m_paused || m_stepFrame;
        if (shouldRender) {
            renderPasses();
            m_stepFrame = false;
        }

        // 渲染到屏幕 (OpenGL)
        renderToScreen();

        // iMouse.w 只表示"本帧刚点击"，所有 pass 都渲染完再清
        if (shouldRender) {
            m_input.clearJustClicked();
        }

        // ImGui 更新和渲染
        if (m_enableGui) {
            // 保存 OpenGL 状态
            m_window.pushGLStates();

            ImGui::SFML::Update(m_window, imguiDeltaClock.restart());
            m_overlay->render();
            ImGui::SFML::Render(m_window);

            // 恢复 OpenGL 状态
            m_window.popGLStates();
        }

        // 统一调用 display()
        m_window.display();

        if (m_options.showFps) {
            std::cout << "\rFPS: " << std::fixed << std::setprecision(1) << m_currentFps << "   " << std::flush;
        }
    }

    if (m_enableGui) {
        m_overlay->shutdown();
    }

    m_exporter.writeAudioDump();
}

void ShadertoyEmulator::runImages() {
    const FrameRange& range = m_options.imageRange;
    const auto exportStart = std::chrono::steady_clock::now();

    if (!ensureDir(m_options.imageDir)) return;

    std::cout << "Exporting images: frames [" << range.start << ", " << range.stop
              << ") step " << range.step << " (" << range.count() << " images) -> "
              << std::filesystem::absolute(m_options.imageDir) << std::endl;

    // 要一并导出的 buffer，名字在这里一次性解析成指针，免得每帧去查一遍 map
    std::vector<RenderPass*> dumpTargets;
    bool dumpAll = false;
    for (const std::string& name : m_options.dumpBuffers) {
        if (name == "all") { dumpAll = true; continue; }

        RenderPass* pass = m_core.findPass(name);
        // 名字写错就说一声。跳过而不报错是为了让一次列一串名字时还能导其余的，
        // 但完全不吭声会让人以为导出了
        if (!pass || pass->isImage || pass->isSound) {
            std::cerr << "--dump-buffers: no buffer pass named \"" << name << "\", skipped"
                      << std::endl;
            continue;
        }
        dumpTargets.push_back(pass);
    }
    if (dumpAll) {
        for (auto& pass : m_core.passes()) {
            if (!pass->isImage && !pass->isSound) dumpTargets.push_back(pass.get());
        }
    }

    int saved = 0;
    while (m_frameCount < range.stop) {
        renderPasses();
        renderToScreen();

        if (range.contains(m_frame.frameIndex)) {
            m_exporter.captureFrame(m_frame.frameIndex);
            // buffer 也只导存盘的那几帧：--images 0:300:1 配 4 个 buffer 就是上千个文件
            for (RenderPass* pass : dumpTargets) {
                m_exporter.captureBuffer(*pass, m_frame.frameIndex);
            }
            saved++;
        }
    }

    // 每帧成本按实际渲染的帧数（range.stop）算，不是存盘张数：--images 0:300:2 只存
    // 150 张，但 300 帧一帧不少地渲染了，拿存盘张数去除会把成本算高一倍
    m_exporter.finish(std::to_string(saved) + " frames", range.stop, exportStart);
}

void ShadertoyEmulator::runVideo() {
    const int fps = static_cast<int>(m_options.fps + 0.5f);
    const int totalFrames = m_options.videoSeconds * fps;
    const auto exportStart = std::chrono::steady_clock::now();

    if (!ensureDir(m_options.videoPath.parent_path())) return;

    // 有 Sound pass 才建音轨；没有就只写画面
    if (!m_exporter.beginVideo(m_core.hasSoundPass())) {
        return;
    }

    std::cout << "Exporting video: " << totalFrames << " frames (" << m_options.videoSeconds
              << "s @ " << fps << "fps) -> "
              << std::filesystem::absolute(m_options.videoPath) << std::endl;

    while (m_frameCount < totalFrames) {
        renderPasses();
        renderToScreen();

        m_exporter.writeVideoFrame();
    }

    m_exporter.endVideo();

    m_exporter.finish("video: " + std::to_string(totalFrames) + " frames",
                      totalFrames, exportStart);
}

void ShadertoyEmulator::handleEvents() {
    bool hadEvent = false;

    while (auto event = m_window.pollEvent()) {
        if (event.has_value()) {
            // ImGui 事件处理（优先）
            if (m_enableGui) {
                ImGui::SFML::ProcessEvent(m_window, *event);

                // 只让渡鼠标/键盘事件。Closed、Resized 这类窗口生命周期事件被吞会导致
                // 鼠标停在面板上时点 X 关不掉窗口、ESC 也退不出去。
                // 鼠标释放同样放行，否则拖拽中划过面板再松开会让按下状态卡住。
                const ImGuiIO& io = ImGui::GetIO();
                bool imGuiWants = false;
                if (event->is<sf::Event::KeyPressed>() || event->is<sf::Event::KeyReleased>()) {
                    imGuiWants = io.WantCaptureKeyboard;
                } else if (event->is<sf::Event::MouseMoved>() || event->is<sf::Event::MouseButtonPressed>()) {
                    imGuiWants = io.WantCaptureMouse;
                }
                if (imGuiWants) {
                    continue;
                }
            }

            if (event->is<sf::Event::Closed>()) {
                m_window.close();
            }
            else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
                // 拖拽时才算有意义的事件
                if (m_input.onMouseMoved(mouseMoved->position.x, mouseMoved->position.y, m_height)) {
                    hadEvent = true;
                }
            }
            else if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                hadEvent = true;
                m_input.onMousePressed(mousePressed->position.x, mousePressed->position.y, m_height);
            }
            else if (event->is<sf::Event::MouseButtonReleased>()) {
                hadEvent = true;
                m_input.onMouseReleased();
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                hadEvent = true;
                m_width = static_cast<int>(resized->size.x);
                m_height = static_cast<int>(resized->size.y);
                m_window.setView(sf::View(sf::FloatRect({0.0f, 0.0f},
                                          sf::Vector2f(static_cast<float>(m_width),
                                                      static_cast<float>(m_height)))));
                m_core.resize(m_width, m_height);
                m_exporter.setOutputTarget(m_core.outputTarget(), m_width, m_height);
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                hadEvent = true;
                m_input.setKey(keyPressed->code, keyPressed->scancode, true);
                if (keyPressed->code == sf::Keyboard::Key::Escape) {
                    m_window.close();
                }
            }
            else if (const auto* keyReleased = event->getIf<sf::Event::KeyReleased>()) {
                hadEvent = true;
                m_input.setKey(keyReleased->code, keyReleased->scancode, false);
            }
        }
    }

    // 暂停时，有事件才步进一帧
    if (m_paused && hadEvent) {
        m_stepFrame = true;
    }
}

void ShadertoyEmulator::beginFrame() {
    if (m_options.isOffscreen()) {
        // 离屏没有 vsync 限帧，用虚拟时间，否则 iTime 几乎不涨、动画会静止
        m_frame.time = static_cast<float>(m_frameCount) / m_options.fps;
        m_frame.timeDelta = 1.0f / m_options.fps;
        // iDate 也用固定纪元 + 虚拟秒，否则同一命令每次导出的结果都不一样
        m_frame.date = {2024.0f, 1.0f, 1.0f, std::fmod(m_frame.time, 86400.0f)};
    } else if (m_paused) {
        m_frame.time = m_pausedTime;
        m_frame.timeDelta = m_pausedTimeDelta;  // 使用保存的值
        // 暂停期间 iDate 保持上一次的值
    } else {
        auto now = std::chrono::high_resolution_clock::now();
        m_frame.time = std::chrono::duration<float>(now - m_startTime).count();
        m_frame.timeDelta = std::chrono::duration<float>(now - m_lastFrameTime).count();

        std::time_t t = std::time(nullptr);
        std::tm* tm = std::localtime(&t);
        m_frame.date = {static_cast<float>(tm->tm_year + 1900),
                        static_cast<float>(tm->tm_mon + 1),
                        static_cast<float>(tm->tm_mday),
                        static_cast<float>(tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec)};
    }

    // 本帧帧号的快照，别删。m_frameCount 在 renderPasses() 末尾就自增了，而
    // renderToScreen() 和截图都发生在那之后，它们要的是"刚渲染的这一帧"的号；
    // 直接用 m_frameCount 会让 Image pass 的 iFrame 比 Buffer pass 多 1，导出文件名也整体偏移
    m_frame.frameIndex = m_frameCount;
    m_frame.mouse = sf::Glsl::Vec4(m_input.mouseX(), m_input.mouseY(),
                                   m_input.mouseZ(), m_input.mouseW());
}

void ShadertoyEmulator::renderPasses() {
    // 先定好本帧的时间，后面的 buffer pass 和 image pass 都用这一份，
    // 否则 m_frameCount 自增会让两者差一整帧
    beginFrame();

    m_core.updateKeyboard(m_input.keys());
    m_core.renderBufferPasses(m_frame);

    // 检查并生成声音（此时可以读取当前帧的 Buffer 数据）
    checkAndGenerateSound();

    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    m_frameCount++;
}

void ShadertoyEmulator::renderToScreen() {
    m_core.renderToScreen(m_frame, m_debugViewPass);

    // 离屏模式没有窗口可贴
    if (!m_options.isOffscreen()) {
        presentToWindow();
    }
    // display() 由 run() 统一调用，以支持 ImGui
}

void ShadertoyEmulator::presentToWindow() {
    const sf::Vector2u windowSize = m_window.getSize();
    const int dstW = static_cast<int>(windowSize.x);
    const int dstH = static_cast<int>(windowSize.y);
    if (dstW <= 0 || dstH <= 0) return;  // 最小化时窗口的 framebuffer 是 0×0

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_core.outputTarget()->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);  // blit 受 draw framebuffer 的 scissor 影响，ImGui 可能留下状态
    glBlitFramebuffer(0, 0, m_width, m_height,
                      0, 0, dstW, dstH,
                      GL_COLOR_BUFFER_BIT,
                      (dstW == m_width && dstH == m_height) ? GL_NEAREST : GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);  // 恢复默认，ImGui/SFML 需要 FBO 0
}

void ShadertoyEmulator::checkAndGenerateSound() {
    if (!m_core.hasSoundPass()) return;

    if (m_options.isOffscreen()) {
        // 导出模式没有播放消耗，每帧固定生成一批
        generateSoundBatch();
        return;
    }

    if (!m_soundStream) return;

    // 保持至少 5 个就绪缓冲区
    if (m_soundStream->getReadyBufferCount() < 5) {
        generateSoundBatch();
    }
}

void ShadertoyEmulator::generateSoundBatch() {
    if (!m_core.hasSoundPass()) return;

    if (m_options.isOffscreen() && m_options.fps > 0.0f) {
        // 导出模式要把音频对齐到视频时间轴：第 N 帧结束时正好走到 N+1 帧对应的时间点。
        // 用目标位置减当前位置，44100/fps 除不尽（24fps 是 1837.5）也不会越积越偏
        const int64_t target = static_cast<int64_t>(
            static_cast<double>(m_frameCount + 1) * SOUND_SAMPLE_RATE / m_options.fps);
        // 帧率低时一帧要的样本可能超过 FBO 一次能渲染的量，拆成几批补完
        int remaining = static_cast<int>(target - m_soundSamplePosition);
        while (remaining > 0) {
            const int batchSamples = std::min(remaining, m_core.soundBatchSamples());
            renderSoundBatch(batchSamples);
            remaining -= batchSamples;
        }
        return;
    }

    // 窗口模式：跟着播放进度补货
    renderSoundBatch(m_core.soundBatchSamples());
}

// 采样怎么算是 RenderCore 的事，这里只管把结果送去播放、编码和存档
void ShadertoyEmulator::renderSoundBatch(int batchSamples) {
    if (batchSamples <= 0) return;

    SoundState state;
    state.time = static_cast<float>(m_soundSamplePosition) / SOUND_SAMPLE_RATE;
    state.frame = m_frameCount;
    state.sampleOffset = m_soundSamplePosition;
    state.mouse = sf::Glsl::Vec4(m_input.mouseX(), m_input.mouseY(),
                                 m_input.mouseZ(), m_input.mouseW());

    m_core.renderSoundBatch(batchSamples, state, m_audioSamples);

    if (m_soundStream) {
        m_soundStream->pushSamples(m_audioSamples);
    }
    // 视频音轨和 WAV 各判各的去向
    m_exporter.appendAudio(m_audioSamples);
    m_soundSamplePosition += batchSamples;
}

void ShadertoyEmulator::resetShader() {
    m_startTime = std::chrono::high_resolution_clock::now();
    m_lastFrameTime = m_startTime;
    m_frameCount = 0;
    m_pausedTime = 0.0f;
    m_pauseAnchor = 0.0f;
    // 重置后渲染一帧
    m_stepFrame = true;
    // 注意：不修改 m_paused 状态，保持暂停状态

    // 音频也回到起点：丢掉旧缓冲、采样位置归零后重新预生成，
    // 否则画面回到 0 秒而声音还停在原处，音画就错开了。
    // 重建要碰 FBO，不能在 ImGui 的回调里做，交给主循环
    if (m_soundStream) {
        m_audioResyncTarget = 0.0f;
        m_audioResyncPending = true;
    }
}

void ShadertoyEmulator::pausePlayback() {
    if (m_paused) return;

    m_paused = true;
    // 暂停画面时声音也得停，不然恢复之后音画就错开了
    if (m_soundStream) m_soundStream->pause();

    auto now = std::chrono::high_resolution_clock::now();
    m_pausedTime = std::chrono::duration<float>(now - m_startTime).count();
    m_pausedTimeDelta = std::chrono::duration<float>(now - m_lastFrameTime).count();
    // 记下暂停点：恢复时靠它判断中间有没有单帧步进过
    m_pauseAnchor = m_pausedTime;
}

void ShadertoyEmulator::resyncAudio(float targetTime) {
    if (!m_soundStream) return;

    m_soundStream->stop();  // 内部会重置播放位置并清掉 SFML 侧的缓冲
    m_soundStream->clearQueue();
    m_soundSamplePosition = static_cast<int64_t>(targetTime * SOUND_SAMPLE_RATE);
    for (int i = 0; i < 6; ++i) {
        generateSoundBatch();
    }
    // 暂停状态下不重新起播，等恢复时一起继续
    if (!m_paused) m_soundStream->play();
}
