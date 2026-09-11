#pragma once

#include <SFML/Window/Keyboard.hpp>
#include <array>

// Shadertoy 的鼠标与键盘状态。这里不碰窗口也不碰 GL——主循环把 sf::Event 翻译成
// 下面的调用，要用哪份数据就取哪份
class InputState {
public:
    // 窗口坐标原点在左上、y 向下，Shadertoy 的在左下。翻转在这儿一次做完，
    // 免得每个消费者都各自记得要翻。返回 false 表示这不算一次有意义的输入
    // （没按下时挪鼠标什么也不该发生）
    bool onMouseMoved(int x, int y, int windowHeight);
    void onMousePressed(int x, int y, int windowHeight);
    void onMouseReleased();
    void clearJustClicked() { m_justClicked = false; }

    bool mouseDown() const { return m_down; }

    // iMouse 的 z/w 靠符号位编码状态：负值表示按钮已松开 / 本帧没点击。
    // Image、Buffer、Sound 的语义在 Shadertoy 里是同一套，所以必须共用这份编码
    float mouseX() const { return m_downX; }
    float mouseY() const { return m_downY; }
    float mouseZ() const { return m_down ? m_clickX : -m_clickX; }
    float mouseW() const { return m_justClicked ? m_clickY : -m_clickY; }

    // 先按 Key 查，查不到再按 Scancode 查——Key 枚举里没有的键（大小写锁定、
    // 小键盘回车之类）只能靠后者认出来
    void setKey(sf::Keyboard::Key key, sf::Keyboard::Scancode scancode, bool pressed);
    const std::array<bool, 256>& keys() const { return m_pressed; }

private:
    float m_downX = 0.0f, m_downY = 0.0f;    // 最后一次按下的位置，拖拽时跟着走
    float m_clickX = 0.0f, m_clickY = 0.0f;  // 最后一次点击的位置，按下期间不动
    bool m_down = false;
    bool m_justClicked = false;

    std::array<bool, 256> m_pressed{};
};

// SFML 的键位映射成 Shadertoy 用的 JavaScript keyCode（0-255），认不出的返回 -1
int mapSfmlKeyToShadertoy(sf::Keyboard::Key key);
int mapSfmlScancodeToShadertoy(sf::Keyboard::Scancode scancode);
