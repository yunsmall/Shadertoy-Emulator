#include "input_state.hpp"

bool InputState::onMouseMoved(int x, int y, int windowHeight) {
    if (!m_down) return false;
    m_downX = static_cast<float>(x);
    m_downY = static_cast<float>(windowHeight - y);
    return true;
}

void InputState::onMousePressed(int x, int y, int windowHeight) {
    m_down = true;
    m_justClicked = true;
    m_downX = static_cast<float>(x);
    m_downY = static_cast<float>(windowHeight - y);
    m_clickX = m_downX;
    m_clickY = m_downY;
}

void InputState::onMouseReleased() {
    m_down = false;
}

void InputState::setKey(sf::Keyboard::Key key, sf::Keyboard::Scancode scancode, bool pressed) {
    int code = mapSfmlKeyToShadertoy(key);
    if (code < 0) {
        code = mapSfmlScancodeToShadertoy(scancode);
    }
    if (code >= 0 && code < 256) {
        m_pressed[code] = pressed;
    }
}

int mapSfmlKeyToShadertoy(sf::Keyboard::Key key) {
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

int mapSfmlScancodeToShadertoy(sf::Keyboard::Scancode scancode) {
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
