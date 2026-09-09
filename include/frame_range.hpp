#pragma once

#include <string>
#include <vector>
#include <charconv>

// Python 风格的帧号切片 start:stop:step，语义与 Python 一致：含 start、不含 stop
struct FrameRange {
    int start = 0;
    int stop = 0;
    int step = 1;

    bool contains(int frame) const {
        return frame >= start && frame < stop && (frame - start) % step == 0;
    }

    int count() const {
        if (stop <= start) return 0;
        return (stop - start + step - 1) / step;
    }
};

// 解析 "0:100:2"、":100"、"0:100"、"100"。
// 单数字按 Python 的 slice(100) 解释成 stop。stop 缺失、step <= 0、负数、非数字都返回 false。
// 负索引不支持：帧号的上界事先不知道，没法像 Python 那样从末尾数。
inline bool parseFrameRange(const std::string& text, FrameRange& out) {
    if (text.empty()) return false;

    std::vector<std::string> parts;
    size_t pos = 0;
    while (true) {
        size_t next = text.find(':', pos);
        if (next == std::string::npos) {
            parts.push_back(text.substr(pos));
            break;
        }
        parts.push_back(text.substr(pos, next - pos));
        pos = next + 1;
    }
    if (parts.size() > 3) return false;

    auto parsePart = [](const std::string& s, int& value) {
        if (s.empty()) return false;
        int result = 0;
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
        if (ec != std::errc() || ptr != s.data() + s.size()) return false;
        if (result < 0) return false;
        value = result;
        return true;
    };

    FrameRange range;
    if (parts.size() == 1) {
        if (!parsePart(parts[0], range.stop)) return false;
    } else {
        if (!parts[0].empty() && !parsePart(parts[0], range.start)) return false;
        if (!parsePart(parts[1], range.stop)) return false;  // stop 必填，否则没有退出条件
        if (parts.size() == 3 && !parsePart(parts[2], range.step)) return false;
    }

    if (range.step <= 0) return false;
    if (range.stop <= range.start) return false;

    out = range;
    return true;
}
