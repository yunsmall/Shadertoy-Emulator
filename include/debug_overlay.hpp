#pragma once

#include <SFML/Graphics.hpp>
#include <imgui.h>

class ShadertoyEmulator;

// 调试面板：Controls 窗口、pass 列表、缩略图和像素探针。它就是这个程序的一个视图，
// 直接读写 ShadertoyEmulator 的状态——中间隔一层"接口"只会得到一堆转发函数
class DebugOverlay {
public:
    explicit DebugOverlay(ShadertoyEmulator& emulator);

    // 失败时返回 false，调用方得退回无 GUI 模式，否则后面每帧的 Update/Render 都会崩
    bool init(sf::RenderWindow& window);
    void shutdown();
    void render();

private:
    void drawControls();
    void drawPlaybackButtons(const ImVec2& buttonSize);
    void drawStatus(float labelWidth);
    void drawPixelProbe(float labelWidth);  // 顺带按需刷新探针的值
    void drawPassList();
    void drawThumbnails();

    ShadertoyEmulator& m_emu;
};
