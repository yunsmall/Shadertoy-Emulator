#include "debug_overlay.hpp"
#include "shadertoy_emulator.hpp"
#include <imgui-SFML.h>
#include <glad/glad.h>
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

// 下面三个只给通道列表用
static const char* filterName(ChannelInput::Filter filter) {
    switch (filter) {
        case ChannelInput::Filter::Linear:  return "linear";
        case ChannelInput::Filter::Nearest: return "nearest";
        case ChannelInput::Filter::Mipmap:  return "mipmap";
    }
    return "?";
}

static const char* wrapName(ChannelInput::Wrap wrap) {
    switch (wrap) {
        case ChannelInput::Wrap::Clamp:  return "clamp";
        case ChannelInput::Wrap::Repeat: return "repeat";
        case ChannelInput::Wrap::Mirror: return "mirror";
    }
    return "?";
}

// 通道接的是什么。纹理只留文件名——调试面板就那么宽，完整路径会把窗口撑开
static std::string channelSourceName(const ChannelInput& channel) {
    switch (channel.type) {
        case ChannelInput::Type::Buffer:   return channel.source;
        case ChannelInput::Type::Keyboard: return "keyboard";
        case ChannelInput::Type::Texture:
            return std::filesystem::path(channel.source).filename().string();
    }
    return "?";
}

DebugOverlay::DebugOverlay(ShadertoyEmulator& emulator) : m_emu(emulator) {}

bool DebugOverlay::init(sf::RenderWindow& window) {
    if (!ImGui::SFML::Init(window)) {
        std::cerr << "Failed to initialize ImGui" << std::endl;
        return false;
    }
    ImGui::StyleColorsDark();

    // 默认样式的直角和紧凑间距看着很生硬，加点圆角、留白放宽些
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    return true;
}

void DebugOverlay::shutdown() {
    ImGui::SFML::Shutdown();
}

void DebugOverlay::render() {
    drawControls();
    drawPassList();
}

void DebugOverlay::drawControls() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 190), ImGuiCond_FirstUseEver);
    // 半透明，压在画面上时不至于糊住一大块
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoCollapse);

    const ImGuiStyle& style = ImGui::GetStyle();
    const float buttonWidth = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2.0f) / 3.0f;
    const ImVec2 buttonSize(buttonWidth, 26.0f);

    drawPlaybackButtons(buttonSize);

    ImGui::SeparatorText("Status");

    // 标签列宽度按最长的那个算，几行数值才能对齐成一条
    const float labelWidth = ImGui::CalcTextSize("Resolution").x + style.ItemSpacing.x * 2.0f;
    drawStatus(labelWidth);

    ImGui::SeparatorText("Pixel");
    drawPixelProbe(labelWidth);

    ImGui::End();
}

void DebugOverlay::drawPlaybackButtons(const ImVec2& buttonSize) {
    // 绿=继续走、琥珀=会停下、红=从头来，靠颜色区分比读字快
    if (m_emu.m_paused) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.26f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.58f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.36f, 0.21f, 1.0f));
        const bool resumeClicked = ImGui::Button("Resume", buttonSize);
        ImGui::PopStyleColor(3);

        if (resumeClicked) {
            m_emu.m_paused = false;
            // 恢复时间：调整 startTime 使 iTime 从暂停处继续
            auto now = std::chrono::high_resolution_clock::now();
            m_emu.m_startTime = now -
                std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(
                    std::chrono::duration<float>(m_emu.m_pausedTime));

            if (m_emu.m_pausedTime > m_emu.m_pauseAnchor) {
                // 单帧步进把暂停点往前推过，音频得按新位置重新生成才追得上画面。
                // 音频是时间的确定函数，从同一时刻重生成的内容和原来一致。
                // 重建要碰 FBO，不能在 ImGui 的回调里做，交给主循环
                m_emu.m_audioResyncTarget = m_emu.m_pausedTime;
                m_emu.m_audioResyncPending = true;
            } else if (m_emu.m_soundStream) {
                // 没步进过就直接接着播。走重建那条路要重启音频设备、重填队列，
                // 中间会有一小段没声音
                m_emu.m_soundStream->play();
            }
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.48f, 0.36f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.60f, 0.45f, 0.13f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.38f, 0.29f, 0.08f, 1.0f));
        const bool pauseClicked = ImGui::Button("Pause", buttonSize);
        ImGui::PopStyleColor(3);

        if (pauseClicked) {
            m_emu.pausePlayback();
        }
    }

    ImGui::SameLine();

    // 单帧步进。位置一直占着、非暂停时变灰，免得按钮行随状态左右跳
    if (ImGui::Button("Next Frame", buttonSize)) {
        // 没暂停就先暂停，省得状态不对时点了半天没反应
        m_emu.pausePlayback();
        // 暂停时 iTime 是冻在 m_pausedTime 上的，只重渲染拿到的还是同一帧。
        // 把暂停点往前推一帧，画面才真的往前走
        m_emu.m_pausedTime += (m_emu.m_pausedTimeDelta > 0.0f) ? m_emu.m_pausedTimeDelta
                                                               : (1.0f / 60.0f);
        m_emu.m_stepFrame = true;
    }

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.42f, 0.21f, 0.21f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.27f, 0.27f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.33f, 0.16f, 0.16f, 1.0f));
    if (ImGui::Button("Reset", buttonSize)) {
        m_emu.resetShader();
    }
    ImGui::PopStyleColor(3);
}

void DebugOverlay::drawStatus(float labelWidth) {
    const float currentTime = m_emu.m_paused ? m_emu.m_pausedTime :
        std::chrono::duration<float>(std::chrono::high_resolution_clock::now()
                                     - m_emu.m_startTime).count();

    ImGui::TextDisabled("Time");
    ImGui::SameLine(labelWidth);
    ImGui::Text("%.2f s", currentTime);

    ImGui::TextDisabled("Frame");
    ImGui::SameLine(labelWidth);
    ImGui::Text("%d", m_emu.m_frameCount);

    ImGui::TextDisabled("FPS");
    ImGui::SameLine(labelWidth);
    // 掉到 30 以下标红，卡顿时一眼能看见
    const bool slow = m_emu.m_currentFps < 30.0f;
    if (slow) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.45f, 0.42f, 1.0f));
    ImGui::Text("%.1f", m_emu.m_currentFps);
    if (slow) ImGui::PopStyleColor();

    ImGui::TextDisabled("Resolution");
    ImGui::SameLine(labelWidth);
    ImGui::Text("%d x %d", m_emu.m_width, m_emu.m_height);
}

void DebugOverlay::drawPixelProbe(float labelWidth) {
    ImGui::Checkbox("Probe", &m_emu.m_showPixelProbe);

    // 读的是画面上此刻显示的那份数据：看 buffer 时是原始浮点，负值和超过 1 的都还在；
    // 看 Image 时是输出目标那份 8bit，读出来已经量化过
    const sf::Vector2u winSize = m_emu.m_window.getSize();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    // 鼠标压在 ImGui 窗口上时读到的不是用户想看的那块画面，跳过
    const bool overImage = m_emu.m_showPixelProbe && winSize.x > 0 && winSize.y > 0
        && !ImGui::GetIO().WantCaptureMouse
        && mouse.x >= 0.0f && mouse.y >= 0.0f
        && mouse.x < static_cast<float>(winSize.x) && mouse.y < static_cast<float>(winSize.y);

    if (!overImage) {
        ImGui::TextDisabled("Point at the image to read a pixel");
        return;
    }

    // 窗口可以缩放到和渲染分辨率不同的尺寸，按比例换算回纹理坐标。
    // y 要翻过来：鼠标坐标原点在左上，GL 的在左下
    const int x = std::clamp(
        static_cast<int>(mouse.x / winSize.x * m_emu.m_width), 0, m_emu.m_width - 1);
    const int y = std::clamp(
        static_cast<int>((1.0f - mouse.y / winSize.y) * m_emu.m_height), 0, m_emu.m_height - 1);

    // glReadPixels 是同步的，要等 GPU 把这一帧画完才拿得到值，每帧都读会一直
    // 卡住管线（重负载 shader 上实测 7~12ms，帧预算才 16.7ms）。
    // 鼠标一动马上读——位置变了值肯定也变；不动的时候降频，画面自己在动也追得上
    const bool moved = (mouse.x != m_emu.m_probeLastX || mouse.y != m_emu.m_probeLastY);
    if (moved || ++m_emu.m_probeIdleFrames >= 15) {
        m_emu.m_probeIdleFrames = 0;
        m_emu.m_probeLastX = mouse.x;
        m_emu.m_probeLastY = mouse.y;

        GLFramebuffer* target = m_emu.m_core.currentViewTarget(m_emu.m_debugViewPass);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, target->fbo);
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, m_emu.m_probePixel.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);  // 恢复默认，ImGui/SFML 需要 FBO 0
    }

    ImGui::TextDisabled("At");
    ImGui::SameLine(labelWidth);
    ImGui::Text("%d, %d", x, y);

    // 顺手把颜色画成一块：对着四个数在脑子里拼颜色太费劲。
    // 超范围的分量按夹紧后的值显示，越界这件事交给下面标红的数字去说
    ImGui::SameLine();
    const ImVec2 swatchPos = ImGui::GetCursorScreenPos();
    const float swatchSize = ImGui::GetTextLineHeight();
    const ImVec2 swatchEnd(swatchPos.x + swatchSize, swatchPos.y + swatchSize);
    const ImVec4 swatch(std::clamp(m_emu.m_probePixel[0], 0.0f, 1.0f),
                        std::clamp(m_emu.m_probePixel[1], 0.0f, 1.0f),
                        std::clamp(m_emu.m_probePixel[2], 0.0f, 1.0f), 1.0f);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(swatchPos, swatchEnd, ImGui::ColorConvertFloat4ToU32(swatch));
    drawList->AddRect(swatchPos, swatchEnd, IM_COL32(130, 130, 130, 255));
    ImGui::Dummy(ImVec2(swatchSize, swatchSize));  // 占位，让后面的布局知道这里有东西

    // 逐个分量显示。越界（负值或超过 1）标红——调数值时找的往往就是这两种
    static const char* const componentNames[4] = {"R", "G", "B", "A"};
    for (int i = 0; i < 4; ++i) {
        ImGui::TextDisabled("%s", componentNames[i]);
        ImGui::SameLine(labelWidth);

        const bool outOfRange = m_emu.m_probePixel[i] < 0.0f || m_emu.m_probePixel[i] > 1.0f;
        if (outOfRange) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.45f, 0.42f, 1.0f));
        }
        ImGui::Text("%.4f", m_emu.m_probePixel[i]);
        if (outOfRange) ImGui::PopStyleColor();
    }
}

// 调试视图的 pass 列表。单独开一个窗口：pass 一多，塞进 Controls 会把它撑得老长。
// 缩略图展开后窗口能长到屏幕外面去，限制个最大高度让它自己滚动
void DebugOverlay::drawPassList() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, 700.0f));
    ImGui::SetNextWindowPos(ImVec2(345, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.85f);
    if (ImGui::Begin("Passes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        for (auto& pass : m_emu.m_core.passes()) {
            if (pass->isSound) continue;  // 音频没有能看的画面

            // 看 Image 就等于没开调试视图，其余 pass 得名字对上才算选中
            const bool selected = pass->isImage ? m_emu.m_debugViewPass.empty()
                                                : (m_emu.m_debugViewPass == pass->name);
            // Image 的 width/height 不随窗口缩放更新，显示窗口的才对
            const int w = pass->isImage ? m_emu.m_width : pass->width;
            const int h = pass->isImage ? m_emu.m_height : pass->height;

            const std::string label = pass->name + "   " + std::to_string(w) + "x"
                + std::to_string(h) + "##" + pass->name;
            if (ImGui::RadioButton(label.c_str(), selected)) {
                m_emu.m_debugViewPass = pass->isImage ? "" : pass->name;
            }
        }

        // 名字打错时画面会照常显示 Image，不提示的话很容易以为调试视图没生效
        if (!m_emu.m_debugViewPass.empty()
            && !m_emu.m_core.debugViewPass(m_emu.m_debugViewPass)) {
            ImGui::TextColored(ImVec4(0.92f, 0.45f, 0.42f, 1.0f),
                               "No pass named \"%s\"", m_emu.m_debugViewPass.c_str());
        }

        // 当前看的这个 pass 的输入接了什么。排查"接错通道/漏接"比翻一遍 config 快得多
        RenderPass* viewed = m_emu.m_core.debugViewPass(m_emu.m_debugViewPass);
        if (!viewed) {
            // 没开调试视图时画面上是 Image
            viewed = m_emu.m_core.imagePass();
        }
        if (viewed) {
            // 默认收起：通道表只在排查"接错/漏接"时才看，平时白占四五行
            const std::string header = "Inputs: " + viewed->name + "##inputs";
            if (ImGui::CollapsingHeader(header.c_str())) {
                bool anyInput = false;
                for (int i = 0; i < 4; ++i) {
                    const auto& channel = viewed->channels[i];
                    if (!channel) continue;  // 没接的通道不占行
                    anyInput = true;

                    ImGui::TextDisabled("[%d] %s", i, channelSourceName(*channel).c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s/%s", filterName(channel->filter),
                                        wrapName(channel->wrap));
                }
                if (!anyInput) ImGui::TextDisabled("(no inputs)");
            }
        }

        drawThumbnails();
    }
    ImGui::End();
}

void DebugOverlay::drawThumbnails() {
    // 缩略图默认关着：每个 buffer 每帧都要 blit 一次，不看时白花这份开销
    ImGui::SeparatorText("Thumbnails");
    ImGui::Checkbox("Show", &m_emu.m_showThumbnails);
    if (!m_emu.m_showThumbnails) return;

    int shown = 0;
    for (auto& pass : m_emu.m_core.passes()) {
        // 缩略图是给 buffer 看的。Image 的内容就是画面本身，抬头就能看，
        // 而且它没有自己的 FBO，硬取会拿到空指针
        if (pass->isImage || pass->isSound) continue;

        m_emu.m_core.blitToThumbnail(*pass);

        const auto it = m_emu.m_core.thumbnails().find(pass->name);
        if (it == m_emu.m_core.thumbnails().end() || !it->second) continue;
        const GLTexture& tex = it->second->colorTex;

        // 两个一行：竖着排的话四五个 buffer 就把窗口拉到屏幕外面去了
        if (shown % 2 != 0) ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled("%s", pass->name.c_str());
        // ImGui-SFML 的 ImTextureID 就是 GL 纹理名，直接递给原生 Image 接口。
        // uv 的 y 对调：GL 纹理原点在左下，ImGui 的在左上，不换会上下颠倒
        ImGui::Image(static_cast<ImTextureID>(tex.id),
                     ImVec2(static_cast<float>(tex.width), static_cast<float>(tex.height)),
                     ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        ImGui::EndGroup();
        shown++;
    }
}
